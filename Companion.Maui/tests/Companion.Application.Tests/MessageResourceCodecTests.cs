using System.Collections.Generic;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Projects;
using Companion.Domain.Resources;

namespace Companion.Application.Tests;

public class MessageResourceCodecTests
{
    private readonly ResourceDiscoveryService _discovery = new();
    private readonly ResourceVolumeReader _reader = new();
    private readonly MessageResourceCodec _codec = new();

    [Fact]
    public void Sci11_Message_ParsesOffsets()
    {
        var folder = RepoPaths.GetTemplateGame("SCI1.1");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Message);

        var package = _reader.Read(folder, descriptor, catalog.Version);
        var decoded = _codec.Decode(package);
        _codec.Validate(decoded);
        var encoded = _codec.Encode(decoded);

        Assert.Equal(package.Body, encoded.Body);
        Assert.True(decoded.Metadata.TryGetValue("MessageOffsets", out var value));
        var offsets = Assert.IsAssignableFrom<IReadOnlyList<int>>(value);
        Assert.NotEmpty(offsets);
    }
}
