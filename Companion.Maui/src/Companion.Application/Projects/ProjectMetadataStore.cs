using System.Text.Json;
using System.Text.Json.Serialization;
using Companion.Domain.Projects;

namespace Companion.Application.Projects;

public interface IProjectMetadataStore
{
    Task<ProjectMetadata> LoadAsync(string path, CancellationToken cancellationToken = default);
    Task SaveAsync(string path, ProjectMetadata metadata, CancellationToken cancellationToken = default);
}

public sealed class ProjectMetadataStore : IProjectMetadataStore
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters = { new JsonStringEnumConverter() }
    };

    public async Task<ProjectMetadata> LoadAsync(string path, CancellationToken cancellationToken = default)
    {
        await using var stream = File.OpenRead(path);
        var metadata = await JsonSerializer.DeserializeAsync<ProjectMetadata>(stream, Options, cancellationToken);
        if (metadata is null)
        {
            throw new InvalidOperationException($"File '{path}' did not contain valid project metadata.");
        }
        return metadata;
    }

    public async Task SaveAsync(string path, ProjectMetadata metadata, CancellationToken cancellationToken = default)
    {
        var directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        await using var stream = File.Create(path);
        await JsonSerializer.SerializeAsync(stream, metadata, Options, cancellationToken);
    }
}
