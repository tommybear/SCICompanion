using Companion.Application.Projects;
using Companion.Domain.Projects;
using Companion.Domain.Resources;

namespace Companion.Application.Resources;

public interface IResourceRepository
{
    IReadOnlyList<ResourceDescriptor> ListResources(string gameFolder);
    DecodedResource LoadResource(string gameFolder, ResourceDescriptor descriptor);
}

public sealed class ResourceRepository : IResourceRepository
{
    private readonly IResourceDiscoveryService _discovery;
    private readonly ResourceVolumeReader _volumeReader;
    private readonly IResourceCodecRegistry _codecRegistry;

    public ResourceRepository(IResourceDiscoveryService discovery, ResourceVolumeReader volumeReader, IResourceCodecRegistry codecRegistry)
    {
        _discovery = discovery;
        _volumeReader = volumeReader;
        _codecRegistry = codecRegistry;
    }

    public IReadOnlyList<ResourceDescriptor> ListResources(string gameFolder)
    {
        var catalog = _discovery.Discover(gameFolder);
        return catalog.Resources;
    }

    public DecodedResource LoadResource(string gameFolder, ResourceDescriptor descriptor)
    {
        var catalog = _discovery.Discover(gameFolder);
        var package = _volumeReader.Read(gameFolder, descriptor, catalog.Version);
        var codec = _codecRegistry.GetCodec(package.Header.Type);
        var decoded = codec.Decode(package);
        codec.Validate(decoded);
        return decoded;
    }
}
