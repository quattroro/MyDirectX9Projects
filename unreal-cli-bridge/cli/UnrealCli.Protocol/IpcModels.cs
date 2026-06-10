#nullable enable

using System.Text.Json.Serialization;

namespace UnrealCli.Protocol;

public sealed class CommandEnvelope
{
    [JsonPropertyName("protocolVersion")] public string protocolVersion { get; set; } = "";
    [JsonPropertyName("requestId")] public string requestId { get; set; } = Guid.NewGuid().ToString("N");
    [JsonPropertyName("command")] public string command { get; set; } = "";
    [JsonPropertyName("arguments")] public Dictionary<string, object?>? arguments { get; set; }
    [JsonPropertyName("force")] public bool force { get; set; }
}

public sealed class ResponseEnvelope
{
    [JsonPropertyName("protocolVersion")] public string protocolVersion { get; set; } = "";
    [JsonPropertyName("requestId")] public string requestId { get; set; } = "";
    [JsonPropertyName("status")] public string status { get; set; } = "";
    [JsonPropertyName("data")] public object? data { get; set; }
    [JsonPropertyName("errorCode")] public string? errorCode { get; set; }
    [JsonPropertyName("errorMessage")] public string? errorMessage { get; set; }
    [JsonPropertyName("errorDetail")] public string? errorDetail { get; set; }
    [JsonPropertyName("durationMs")] public long durationMs { get; set; }
    [JsonPropertyName("transport")] public string transport { get; set; } = ProtocolConstants.TransportLive;

    public static ResponseEnvelope Success(string requestId, object? data, long durationMs) => new()
    {
        protocolVersion = ProtocolConstants.ProtocolVersion,
        requestId = requestId,
        status = "ok",
        data = data,
        durationMs = durationMs,
        transport = ProtocolConstants.TransportLive,
    };

    public static ResponseEnvelope Failure(string requestId, string errorCode, string errorMessage, long durationMs, string? errorDetail = null) => new()
    {
        protocolVersion = ProtocolConstants.ProtocolVersion,
        requestId = requestId,
        status = "error",
        errorCode = errorCode,
        errorMessage = errorMessage,
        errorDetail = errorDetail,
        durationMs = durationMs,
        transport = ProtocolConstants.TransportLive,
    };
}

public sealed class InstanceRecord
{
    [JsonPropertyName("projectRoot")] public string projectRoot { get; set; } = "";
    [JsonPropertyName("projectHash")] public string projectHash { get; set; } = "";
    [JsonPropertyName("pipeName")] public string pipeName { get; set; } = "";
    [JsonPropertyName("pid")] public int pid { get; set; }
    [JsonPropertyName("startedAt")] public string startedAt { get; set; } = "";
    [JsonPropertyName("engineVersion")] public string? engineVersion { get; set; }
    [JsonPropertyName("projectName")] public string? projectName { get; set; }
}

public sealed class InstanceRegistry
{
    [JsonPropertyName("instances")] public InstanceRecord[] instances { get; set; } = Array.Empty<InstanceRecord>();
    [JsonPropertyName("activeProjectRoot")] public string? activeProjectRoot { get; set; }
}
