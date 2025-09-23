using System.Collections.Generic;
using System.Linq;
using System.IO;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Resources.Pic;

namespace Companion.Application.Tests;

public class PicResourceCodecTests
{
    private readonly ResourceDiscoveryService _discovery = new();
    private readonly ResourceVolumeReader _reader = new();
    private readonly PicResourceCodec _codec = new();

    [Fact]
    public void Sci0_Pic_RoundTrips()
    {
        var folder = RepoPaths.GetTemplateGame("SCI0");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Pic && PackageFileExists(folder, r.Package));

        var package = _reader.Read(folder, descriptor, catalog.Version);
        var decoded = _codec.Decode(package);
        _codec.Validate(decoded);
        var encoded = _codec.Encode(decoded);

        Assert.True(decoded.Metadata.TryGetValue("PicCommands", out var commandValue));
        Assert.IsAssignableFrom<IReadOnlyList<PicCommand>>(commandValue);

        Assert.Equal(package.Header.CompressedLength, encoded.Header.CompressedLength);
        Assert.Equal(package.Body, encoded.Body);

        using var ms = new MemoryStream();
        encoded.WriteTo(ms);

        var expected = ReadRawResource(folder, descriptor, catalog.Version, package.Header.GetHeaderSize(catalog.Version) + package.Header.CompressedLength);
        Assert.Equal(expected, ms.ToArray());
    }

    [Fact]
    public void Sci11_Pic_RoundTrips()
    {
        var folder = RepoPaths.GetTemplateGame("SCI1.1");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Pic && PackageFileExists(folder, r.Package));

        var package = _reader.Read(folder, descriptor, catalog.Version);
        var decoded = _codec.Decode(package);
        _codec.Validate(decoded);
        var encoded = _codec.Encode(decoded);

        Assert.True(decoded.Metadata.TryGetValue("PicCommands", out var commandValue));
        Assert.IsAssignableFrom<IReadOnlyList<PicCommand>>(commandValue);

        Assert.Equal(package.Header.CompressedLength, encoded.Header.CompressedLength);
        Assert.Equal(package.Body, encoded.Body);

        using var ms = new MemoryStream();
        encoded.WriteTo(ms);

        var expected = ReadRawResource(folder, descriptor, catalog.Version, package.Header.GetHeaderSize(catalog.Version) + package.Header.CompressedLength);
        Assert.Equal(expected, ms.ToArray());
    }

    private static bool PackageFileExists(string folder, int package)
    {
        var path = Path.Combine(folder, $"resource.{package:000}");
        return File.Exists(path);
    }

    private static byte[] ReadRawResource(string folder, ResourceDescriptor descriptor, SCIVersion version, int totalLength)
    {
        var path = Path.Combine(folder, $"resource.{descriptor.Package:000}");
        using var stream = File.OpenRead(path);
        stream.Seek(descriptor.Offset, SeekOrigin.Begin);
        var bytes = new byte[totalLength];
        var read = stream.Read(bytes, 0, totalLength);
        if (read != totalLength)
        {
            Array.Resize(ref bytes, read);
        }
        return bytes;
    }
}
