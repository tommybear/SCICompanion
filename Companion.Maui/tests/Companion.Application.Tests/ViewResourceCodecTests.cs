using System.Collections.Generic;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Projects;
using Companion.Domain.Resources;

namespace Companion.Application.Tests;

public class ViewResourceCodecTests
{
    private readonly ResourceDiscoveryService _discovery = new();
    private readonly ResourceVolumeReader _reader = new();
    private readonly ViewResourceCodec _codec = new();

    [Fact]
    public void Sci11_View_ProvidesHeaderMetadata()
    {
        var folder = RepoPaths.GetTemplateGame("SCI1.1");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.View);

        var package = _reader.Read(folder, descriptor, catalog.Version);
        var decoded = _codec.Decode(package);
        _codec.Validate(decoded);
        var encoded = _codec.Encode(decoded);

        Assert.Equal(package.Body, encoded.Body);
        Assert.True(decoded.Metadata.TryGetValue("ViewHeader", out var value));
        var header = Assert.IsAssignableFrom<byte[]>(value);
        Assert.NotEmpty(header);
    }
}
