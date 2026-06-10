#nullable enable

using System.IO.Pipes;
using System.Text;
using UnrealCli.Protocol;

namespace UnrealCli.Cli.Services;

public sealed class LocalIpcClient
{
    public async Task<ResponseEnvelope> SendAsync(
        InstanceRecord target,
        CommandEnvelope command,
        int timeoutMs,
        CancellationToken cancellationToken)
    {
        using var linkedCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        linkedCts.CancelAfter(timeoutMs);

        await using var stream = new NamedPipeClientStream(".", target.pipeName, PipeDirection.InOut, PipeOptions.Asynchronous);
        await stream.ConnectAsync(linkedCts.Token);
        return await ExchangeAsync(stream, command, linkedCts.Token);
    }

    private static async Task<ResponseEnvelope> ExchangeAsync(Stream stream, CommandEnvelope command, CancellationToken ct)
    {
        using var writer = new StreamWriter(stream, new UTF8Encoding(false), leaveOpen: true);
        using var reader = new StreamReader(stream, Encoding.UTF8, leaveOpen: true);

        command.protocolVersion = ProtocolConstants.ProtocolVersion;
        await writer.WriteLineAsync(ProtocolJson.Serialize(command));
        await writer.FlushAsync(ct);

        string? responseLine = await reader.ReadLineAsync(ct);
        if (string.IsNullOrWhiteSpace(responseLine))
            throw new IOException("Unreal IPC 응답이 비어 있습니다.");

        var response = ProtocolJson.Deserialize<ResponseEnvelope>(responseLine)
            ?? throw new IOException("Unreal IPC 응답을 파싱하지 못했습니다.");

        return EnsureCompatibleResponse(response);
    }

    internal static ResponseEnvelope EnsureCompatibleResponse(ResponseEnvelope response)
    {
        if (string.Equals(response.protocolVersion, ProtocolConstants.ProtocolVersion, StringComparison.Ordinal))
            return response;

        return ResponseEnvelope.Failure(
            response.requestId,
            ProtocolConstants.ErrorProtocolMismatch,
            $"플러그인 버전이 CLI와 호환되지 않습니다. CLI와 플러그인을 함께 업그레이드하세요. (expected {ProtocolConstants.ProtocolVersion}, got {response.protocolVersion})",
            response.durationMs);
    }
}
