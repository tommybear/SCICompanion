using Companion.Domain.Projects;
using Companion.Domain.Resources;

namespace Companion.Application.Tests;

public class CodecRegistryTests
{
    [Fact]
    public void Registry_ThrowsForMissingCodec()
    {
        var registry = new ResourceCodecRegistry(Array.Empty<IResourceCodec>());
        Assert.Throws<KeyNotFoundException>(() => registry.GetCodec(ResourceType.Pic));
    }

    [Fact]
    public void Registry_ReturnsRegisteredCodec()
    {
        var codec = new RawBinaryCodec(ResourceType.Script);
        var registry = new ResourceCodecRegistry(new[] { codec });
        Assert.Equal(codec, registry.GetCodec(ResourceType.Script));
    }
}
