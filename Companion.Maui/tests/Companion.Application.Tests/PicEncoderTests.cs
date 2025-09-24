using System;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Resources.Pic;
using Xunit;

namespace Companion.Application.Tests;

public class PicEncoderTests
{
    [Fact]
    public void EncodeWritesTerminatorForEmptyDocument()
    {
        var encoder = new PicEncoder();
        var document = new PicDocument(
            320,
            190,
            Array.Empty<PicCommand>(),
            Array.Empty<PicStateSnapshot>(),
            new PicStateSnapshot(PicStateFlags.None, 0, 0, 0, 0, 0, 0, 0, 0, 0, Array.Empty<bool>(), Array.Empty<byte[]>(), Array.Empty<byte>(), Array.Empty<ushort>()),
            Array.Empty<byte>(),
            Array.Empty<byte>(),
            Array.Empty<byte>(),
            Array.Empty<byte>());
        var payload = Array.Empty<byte>();

        var result = encoder.Encode(document, SCIVersion.SCI0, payload);

        Assert.Equal(new byte[] { 0xFF }, result);
    }

    [Theory]
    [InlineData("SCI0")]
    [InlineData("SCI1.1")]
    public void EncodeRoundTripsTemplatePics(string versionFolder)
    {
        var discovery = new ResourceDiscoveryService();
        var reader = new ResourceVolumeReader();
        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1),
            new Lzw1CompressionService(2),
            new LzwPicCompressionService(new Lzw1CompressionService(2), 4),
            new DclCompressionService(8, 18, 19, 20),
            new StacCompressionService(32)
        });
        var codec = new PicResourceCodec(registry);
        var encoder = new PicEncoder();

        var folder = RepoPaths.GetTemplateGame(versionFolder);
        var catalog = discovery.Discover(folder);
        var pics = catalog.Resources.Where(r => r.Type == ResourceType.Pic).ToList();

        Assert.NotEmpty(pics);

        foreach (var descriptor in pics)
        {
            var package = reader.Read(folder, descriptor, catalog.Version);
            var decoded = codec.Decode(package);
            Assert.True(decoded.Metadata.TryGetValue("PicDocument", out var docValue));
            var document = Assert.IsType<PicDocument>(docValue);
            Assert.True(decoded.Metadata.TryGetValue("PicDecodedPayload", out var payloadValue));
            var payload = Assert.IsType<byte[]>(payloadValue);

            var encoded = encoder.Encode(document, catalog.Version, payload);

            if (encoded.Length != payload.Length)
            {
                var expectedBase64 = Convert.ToBase64String(payload);
                var actualBase64 = Convert.ToBase64String(encoded);
                var firstPattern = document.Commands.OfType<PicCommand.SetPattern>().FirstOrDefault();
                var patternInfo = firstPattern is null
                    ? "<none>"
                    : $"number={firstPattern.PatternNumber} size={firstPattern.PatternSize} rectangle={firstPattern.IsRectangle} brush={firstPattern.UseBrush}";
                throw new Xunit.Sdk.XunitException($"Length mismatch in {versionFolder} PIC #{descriptor.Number}: expected {payload.Length} bytes, actual {encoded.Length} bytes.\nExpected (base64): {expectedBase64}\nActual   (base64): {actualBase64}\nFirst pattern: {patternInfo}");
            }

            for (var i = 0; i < payload.Length; i++)
            {
                if (payload[i] != encoded[i])
                {
                    throw new Xunit.Sdk.XunitException($"Mismatch in {versionFolder} PIC #{descriptor.Number} at offset {i}: expected {payload[i]:X2}, actual {encoded[i]:X2}");
                }
            }
        }
    }
}
