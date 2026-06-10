#nullable enable

using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using UnrealCli.Protocol;

namespace UnrealCli.Cli.Services;

public sealed class InstanceRegistryStore
{
    private readonly string _registryPath;

    public InstanceRegistryStore()
    {
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        _registryPath = Path.Combine(appData, "UnrealCliBridge", "instances.json");
    }

    public InstanceRegistry Load() => InstanceRegistryFile.Load(_registryPath);

    public void Save(InstanceRegistry registry) => InstanceRegistryFile.Save(_registryPath, registry);

    public void Update(Func<InstanceRegistry, InstanceRegistry> update) =>
        InstanceRegistryFile.Update(_registryPath, update);

    public InstanceRecord? ResolveTarget(string? projectOverride)
    {
        var registry = Load();
        var live = GetLiveInstances(registry.instances);

        if (projectOverride != null)
            return ResolveByOverride(projectOverride, live);

        // CWD match
        string cwd = Directory.GetCurrentDirectory();
        var cwdMatch = FindByProjectRoot(live, cwd);
        if (cwdMatch != null) return cwdMatch;

        // Pinned active project
        if (!string.IsNullOrWhiteSpace(registry.activeProjectRoot))
        {
            var pinned = FindByProjectRoot(live, registry.activeProjectRoot);
            if (pinned != null) return pinned;
        }

        // Single live instance
        if (live.Count == 1) return live[0];

        if (live.Count == 0)
            throw new CliUsageException("실행 중인 Unreal 에디터 인스턴스가 없습니다. 에디터를 먼저 실행하세요.");

        var candidates = string.Join("\n", live.Select(i => $"  {i.projectHash}  {i.projectRoot}"));
        throw new CliUsageException(
            $"여러 Unreal 에디터 인스턴스가 실행 중입니다. --project 또는 `instances use`로 대상을 지정하세요:\n{candidates}");
    }

    private InstanceRecord? ResolveByOverride(string projectOverride, List<InstanceRecord> live)
    {
        if (Directory.Exists(projectOverride))
        {
            string canonical = Path.GetFullPath(projectOverride);
            return FindByProjectRoot(live, canonical)
                ?? throw new CliUsageException($"해당 프로젝트에서 실행 중인 에디터를 찾을 수 없습니다: {canonical}");
        }

        var hashMatch = live.FirstOrDefault(i =>
            string.Equals(i.projectHash, projectOverride, StringComparison.OrdinalIgnoreCase) ||
            i.projectHash.StartsWith(projectOverride, StringComparison.OrdinalIgnoreCase));

        if (hashMatch != null) return hashMatch;

        var nameMatch = live.Where(i =>
            string.Equals(i.projectName, projectOverride, StringComparison.OrdinalIgnoreCase)).ToList();

        return nameMatch.Count switch
        {
            1 => nameMatch[0],
            > 1 => throw new CliUsageException($"프로젝트 이름 `{projectOverride}`이 여러 인스턴스와 일치합니다. 전체 경로를 사용하세요."),
            _ => throw new CliUsageException($"프로젝트를 찾을 수 없습니다: {projectOverride}"),
        };
    }

    private static List<InstanceRecord> GetLiveInstances(InstanceRecord[] instances)
    {
        var live = new List<InstanceRecord>();
        foreach (var instance in instances)
        {
            if (IsAlive(instance))
                live.Add(instance);
        }
        return live;
    }

    private static bool IsAlive(InstanceRecord instance)
    {
        try
        {
            using var process = Process.GetProcessById(instance.pid);
            return !process.HasExited;
        }
        catch
        {
            return false;
        }
    }

    private static InstanceRecord? FindByProjectRoot(List<InstanceRecord> instances, string projectRoot)
    {
        string canonical = Path.GetFullPath(projectRoot);
        return instances.FirstOrDefault(i =>
            string.Equals(Path.GetFullPath(i.projectRoot), canonical, StringComparison.OrdinalIgnoreCase));
    }

    public void SetActiveProject(string? projectRoot)
    {
        Update(reg =>
        {
            reg.activeProjectRoot = projectRoot;
            return reg;
        });
    }

    public static string ComputeProjectHash(string projectRoot)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(projectRoot.ToLowerInvariant().Replace('\\', '/'));
        byte[] hash = SHA256.HashData(bytes);
        return Convert.ToHexString(hash)[..12].ToLowerInvariant();
    }
}
