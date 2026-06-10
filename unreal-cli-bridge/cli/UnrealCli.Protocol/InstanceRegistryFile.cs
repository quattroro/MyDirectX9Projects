#nullable enable

using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Text.Json;

namespace UnrealCli.Protocol;

public static class InstanceRegistryFile
{
    private const int MaxRetryCount = 10;
    private const int RetryDelayMs = 30;
    private const int StaleLockSeconds = 30;

    public static InstanceRegistry Load(string filePath)
    {
        string fullPath = EnsureDirectory(filePath);
        Exception? lastException = null;

        for (int attempt = 0; attempt < MaxRetryCount; attempt++)
        {
            try
            {
                return LoadUnlocked(fullPath);
            }
            catch (IOException ex)
            {
                lastException = ex;
                Thread.Sleep((attempt + 1) * RetryDelayMs);
            }
            catch (JsonException ex)
            {
                lastException = ex;
                Thread.Sleep((attempt + 1) * RetryDelayMs);
            }
        }

        throw lastException ?? new IOException("Failed to load instance registry: " + fullPath);
    }

    public static void Save(string filePath, InstanceRegistry registry)
    {
        string fullPath = EnsureDirectory(filePath);
        WithExclusiveLock(fullPath, () => WriteAtomically(fullPath, NormalizeRegistry(registry)));
    }

    public static void Update(string filePath, Func<InstanceRegistry, InstanceRegistry> update)
    {
        string fullPath = EnsureDirectory(filePath);
        WithExclusiveLock(fullPath, () =>
        {
            InstanceRegistry current = LoadUnlocked(fullPath);
            InstanceRegistry next = NormalizeRegistry(update(current));
            WriteAtomically(fullPath, next);
        });
    }

    private static void WithExclusiveLock(string fullPath, Action action)
    {
        string lockPath = fullPath + ".lock";
        IOException? lastException = null;

        for (int attempt = 0; attempt < MaxRetryCount; attempt++)
        {
            bool acquired = TryAcquireLock(lockPath, out FileStream? lockStream);

            if (acquired && lockStream != null)
            {
                try
                {
                    action();
                    return;
                }
                catch (IOException ex)
                {
                    lastException = ex;
                }
                finally
                {
                    ReleaseLock(lockStream, lockPath);
                }
            }
            else
            {
                lastException = new IOException("Failed to acquire registry lock: " + lockPath);
                TryReclaimStaleLock(lockPath);
            }

            Thread.Sleep((attempt + 1) * RetryDelayMs);
        }

        throw lastException ?? new IOException("Registry lock timeout");
    }

    private static bool TryAcquireLock(string lockPath, out FileStream? lockStream)
    {
        try
        {
            var stream = new FileStream(lockPath, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None);
            int pid = Environment.ProcessId;
            string content = pid.ToString(CultureInfo.InvariantCulture)
                + Environment.NewLine
                + DateTimeOffset.UtcNow.ToString("O", CultureInfo.InvariantCulture)
                + Environment.NewLine;
            byte[] bytes = Encoding.UTF8.GetBytes(content);
            stream.Write(bytes, 0, bytes.Length);
            stream.Flush();
            lockStream = stream;
            return true;
        }
        catch (IOException)
        {
            lockStream = null;
            return false;
        }
    }

    private static bool TryReclaimStaleLock(string lockPath)
    {
        FileStream stream;
        try
        {
            stream = new FileStream(lockPath, FileMode.Open, FileAccess.ReadWrite, FileShare.Delete);
        }
        catch (Exception ex) when (ex is FileNotFoundException or DirectoryNotFoundException or IOException or UnauthorizedAccessException)
        {
            return false;
        }

        string graveyardPath = lockPath + "." + Guid.NewGuid().ToString("N") + ".stale";
        using (stream)
        {
            if (!IsOpenedLockStale(lockPath, stream))
                return false;
            try
            {
                File.Move(lockPath, graveyardPath);
            }
            catch (IOException)
            {
                return false;
            }
        }

        TryDeleteFile(graveyardPath);
        return true;
    }

    private static bool IsOpenedLockStale(string lockPath, FileStream stream)
    {
        DateTime lastWriteUtc;
        try
        {
            lastWriteUtc = File.GetLastWriteTimeUtc(lockPath);
        }
        catch
        {
            return false;
        }

        if (DateTime.UtcNow - lastWriteUtc < TimeSpan.FromSeconds(StaleLockSeconds))
            return false;

        try
        {
            stream.Position = 0;
            using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true, bufferSize: 1024, leaveOpen: true);
            string? ownerPidLine = reader.ReadLine();
            if (ownerPidLine == null || !int.TryParse(ownerPidLine, NumberStyles.Integer, CultureInfo.InvariantCulture, out int ownerPid))
                return true;

            string? lockTimestampLine = reader.ReadLine();
            if (lockTimestampLine == null
                || !DateTimeOffset.TryParseExact(lockTimestampLine, "O", CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out DateTimeOffset lockTimestamp))
                return true;

            return IsProcessStale(ownerPid, lockTimestamp);
        }
        catch
        {
            return false;
        }
    }

    private static bool IsProcessStale(int ownerPid, DateTimeOffset lockTimestamp)
    {
        try
        {
            using var process = Process.GetProcessById(ownerPid);
            if (process.HasExited)
                return true;
            try
            {
                return process.StartTime.ToUniversalTime() > lockTimestamp.UtcDateTime.AddSeconds(1);
            }
            catch
            {
                return false;
            }
        }
        catch (ArgumentException)
        {
            return true;
        }
    }

    private static void ReleaseLock(FileStream lockStream, string lockPath)
    {
        try { lockStream.Dispose(); } finally { TryDeleteFile(lockPath); }
    }

    private static void WriteAtomically(string fullPath, InstanceRegistry registry)
    {
        string tempPath = fullPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            using (var stream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None))
            using (var writer = new StreamWriter(stream, new UTF8Encoding(false)))
                writer.Write(ProtocolJson.Serialize(registry, pretty: true));

            if (File.Exists(fullPath))
                File.Replace(tempPath, fullPath, null);
            else
                File.Move(tempPath, fullPath);
        }
        finally
        {
            if (File.Exists(tempPath))
                File.Delete(tempPath);
        }
    }

    private static InstanceRegistry LoadUnlocked(string fullPath)
    {
        if (!File.Exists(fullPath))
            return new InstanceRegistry();

        using var stream = new FileStream(fullPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
        string json = reader.ReadToEnd();
        if (string.IsNullOrWhiteSpace(json))
            return new InstanceRegistry();

        return NormalizeRegistry(ProtocolJson.Deserialize<InstanceRegistry>(json));
    }

    private static InstanceRegistry NormalizeRegistry(InstanceRegistry? registry)
    {
        registry ??= new InstanceRegistry();
        registry.instances ??= Array.Empty<InstanceRecord>();
        return registry;
    }

    private static string EnsureDirectory(string filePath)
    {
        string fullPath = Path.GetFullPath(filePath);
        string? dir = Path.GetDirectoryName(fullPath);
        if (!string.IsNullOrWhiteSpace(dir))
            Directory.CreateDirectory(dir);
        return fullPath;
    }

    private static void TryDeleteFile(string path)
    {
        try { if (File.Exists(path)) File.Delete(path); } catch { }
    }
}
