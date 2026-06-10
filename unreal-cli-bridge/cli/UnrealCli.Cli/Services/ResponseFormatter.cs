#nullable enable

using System.Text.Json;
using UnrealCli.Cli.Models;
using UnrealCli.Protocol;

namespace UnrealCli.Cli.Services;

public static class ResponseFormatter
{
    public static string Format(ResponseEnvelope response, OutputMode mode)
    {
        return mode switch
        {
            OutputMode.Json => ProtocolJson.Serialize(response, pretty: true),
            OutputMode.Compact => FormatCompact(response),
            _ => FormatDefault(response),
        };
    }

    public static string FormatError(string errorCode, string errorMessage, OutputMode mode)
    {
        var response = ResponseEnvelope.Failure(
            requestId: Guid.NewGuid().ToString("N"),
            errorCode: errorCode,
            errorMessage: errorMessage,
            durationMs: 0);

        return mode switch
        {
            OutputMode.Json => ProtocolJson.Serialize(response, pretty: true),
            OutputMode.Compact => $"{{\"error\":\"{Escape(errorMessage)}\",\"errorCode\":\"{errorCode}\"}}",
            _ => $"error [{errorCode}]: {errorMessage}",
        };
    }

    private static string FormatDefault(ResponseEnvelope response)
    {
        if (response.status == "error")
            return $"error [{response.errorCode}]: {response.errorMessage}"
                + (response.errorDetail != null ? $"\n{response.errorDetail}" : "");

        if (response.data == null)
            return "ok";

        string json = response.data is JsonElement je
            ? je.ToString()
            : ProtocolJson.Serialize(response.data, pretty: true);

        return json;
    }

    private static string FormatCompact(ResponseEnvelope response)
    {
        if (response.status == "error")
            return $"{{\"error\":\"{Escape(response.errorMessage ?? "")}\",\"errorCode\":\"{response.errorCode}\"}}";

        if (response.data == null)
            return "{}";

        return response.data is JsonElement je
            ? je.GetRawText()
            : ProtocolJson.Serialize(response.data);
    }

    private static string Escape(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r");
}
