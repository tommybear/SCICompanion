using System.Text.Json.Serialization;

namespace Companion.Domain.Projects;

public enum InterpreterProfileKind
{
    DosBox,
    ScummVM,
    Custom
}

public sealed record InterpreterProfile(
    InterpreterProfileKind Kind,
    string Executable,
    string Arguments
);

public sealed record ProjectMetadata
{
    [JsonPropertyOrder(0)]
    public required string Title { get; init; }

    [JsonPropertyOrder(1)]
    public required string GameFolder { get; init; }

    [JsonPropertyOrder(2)]
    public required SCIVersion Version { get; init; }

    [JsonPropertyOrder(3)]
    public required InterpreterProfile Interpreter { get; init; }

    [JsonPropertyOrder(4)]
    public string? Notes { get; init; }
}
