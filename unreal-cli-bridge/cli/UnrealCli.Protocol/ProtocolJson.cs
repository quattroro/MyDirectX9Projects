#nullable enable

using System.Text.Json;
using System.Text.Json.Serialization;

namespace UnrealCli.Protocol;

public static class ProtocolJson
{
    private static readonly JsonSerializerOptions Options = new()
    {
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        WriteIndented = false,
        PropertyNameCaseInsensitive = true,
    };

    private static readonly JsonSerializerOptions PrettyOptions = new()
    {
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
    };

    public static string Serialize<T>(T value, bool pretty = false) =>
        JsonSerializer.Serialize(value, pretty ? PrettyOptions : Options);

    public static T? Deserialize<T>(string json) =>
        JsonSerializer.Deserialize<T>(json, Options);

    public static JsonElement ParseElement(string json) =>
        JsonSerializer.Deserialize<JsonElement>(json, Options);
}
