using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Resources;
using Companion.Domain.Compression;

namespace Companion.Application.Tests;

public class ResourceRepositoryTests
{
    private readonly IResourceRepository _repository;

    public ResourceRepositoryTests()
    {
        var discovery = new ResourceDiscoveryService();
        var volumeReader = new ResourceVolumeReader();
        var codecRegistry = new ResourceCodecRegistry(new IResourceCodec[]
        {
            new PicResourceCodec(new CompressionRegistry(new ICompressionService[] { new PassthroughCompressionService(0, 20), new LzwCompressionService(1) }))
        }, type => new RawBinaryCodec(type));
        _repository = new ResourceRepository(discovery, volumeReader, codecRegistry);
    }

    [Fact]
    public void ListResources_ReturnsEntries()
    {
        var resources = _repository.ListResources(RepoPaths.GetTemplateGame("SCI0"));
        Assert.NotEmpty(resources);
    }

    [Fact]
    public void LoadResource_PicDecoded()
    {
        var folder = RepoPaths.GetTemplateGame("SCI0");
        var descriptor = _repository.ListResources(folder).First(r => r.Type == Domain.Projects.ResourceType.Pic);
        var decoded = _repository.LoadResource(folder, descriptor);
        Assert.Equal(Domain.Projects.ResourceType.Pic, decoded.ResourceType);
        Assert.NotEmpty(decoded.Payload);
    }
}
