using System;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Xunit;

namespace Companion.Application.Tests;

public class CompressionFixtureTests
{
    [Fact]
    public void Sci0_Pic_MatchesFixture()
    {
        var data = DecompressPic("SCI0");
        var expected = Convert.FromBase64String("/gEAABEiM0RVZneImaq7zN3uiIgBAgMEBQaIiPn6+/z9/v8IkSo7TF1uiP4BAQABAgMEBQYHCIGCg4SF5oeHcXJzdHV2eIeJiouMjY6P+PH6O/z9/vj+AQIAURIjJGXGNwhZGissbc64iNGSo6RlRjh42ZqrrG1OP3+U2uu8TVSI/gEDYGFiY2RlZmfo6err7O3u6EhBQkNERUZIyMnKy8zNzs8YERoLHB0eGPkA8fP8/wAwEyU=");
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_Pic_MatchesFixture()
    {
        var data = DecompressPic("SCI1.1");
        var expected = Convert.FromBase64String("JgAQDgAAoADw2EYACQAAAF0AAAAAAAAAAAAAAEoAAABEAAAAAAAAACoANQBAAEoAVQBfAGoAdAB/AIoAlACfAKkAtAAAAw0AAAAMAPjR4IlG+rgBAAAAAAUJAAAA8ADz/PgAcmb/");
        Assert.Equal(expected, data);
    }

    private static byte[] DecompressPic(string folderName)
    {
        var discovery = new ResourceDiscoveryService();
        var reader = new ResourceVolumeReader();
        var registry = BuildCompressionRegistry();
        var codec = new PicResourceCodec(registry);

        var folder = RepoPaths.GetTemplateGame(folderName);
        var catalog = discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Pic);
        var package = reader.Read(folder, descriptor, catalog.Version);
        var decoded = codec.Decode(package);
        return decoded.Metadata.TryGetValue("PicDecodedPayload", out var value) && value is byte[] payload
            ? payload
            : Array.Empty<byte>();
    }

    private static CompressionRegistry BuildCompressionRegistry()
    {
        var lzw1 = new Lzw1CompressionService(2);
        return new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1),
            lzw1,
            new LzwViewCompressionService(lzw1, 3),
            new LzwPicCompressionService(lzw1, 4),
            new DclCompressionService(8, 18, 19, 20),
            new StacCompressionService(32)
        });
    }
}
