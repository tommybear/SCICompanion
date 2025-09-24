using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Resources.Pic;
using Xunit;

namespace Companion.Application.Tests;

public class PicPhaseTwoValidationTests
{
    private readonly ResourceDiscoveryService _discovery = new();
    private readonly ResourceVolumeReader _reader = new();
    private readonly CompressionRegistry _registry;

    public PicPhaseTwoValidationTests()
    {
        var passthrough = new PassthroughCompressionService(0);
        var lzw = new LzwCompressionService(1);
        var lzw1 = new Lzw1CompressionService(2);
        var lzwView = new LzwViewCompressionService(lzw1, 3);
        var lzwPic = new LzwPicCompressionService(lzw1, 4);
        var dcl = new DclCompressionService(8, 18, 19, 20);
        var stac = new StacCompressionService(32);
        _registry = new CompressionRegistry(new ICompressionService[] { passthrough, lzw, lzw1, lzwView, lzwPic, dcl, stac });
    }

    [Fact]
    public void TemplatePicsDecodeWithoutErrors()
    {
        foreach (var versionFolder in new[] { "SCI0", "SCI1.1" })
        {
            var folder = RepoPaths.GetTemplateGame(versionFolder);
            var catalog = _discovery.Discover(folder);
            var descriptors = catalog.Resources.Where(r => r.Type == ResourceType.Pic).ToList();
            Assert.NotEmpty(descriptors);

            foreach (var descriptor in descriptors)
            {
                var payload = LoadPicPayload(folder, descriptor, catalog.Version);
                var result = PicParser.Parse(payload, catalog.Version);

                Assert.NotNull(result);
                Assert.NotNull(result.FinalState);
                Assert.Equal(result.Commands.Count, result.StateTimeline.Count);
            }
        }
    }

    [Fact]
    public void Sci0_TemplatePicSnapshotMatches()
    {
        var folder = RepoPaths.GetTemplateGame("SCI0");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Pic);
        var payload = LoadPicPayload(folder, descriptor, catalog.Version);
        var result = PicParser.Parse(payload, catalog.Version);

        var snapshot = result.FinalState;
        var counts = PicParser.AggregateOpcodeCounts(result.Commands);

        Assert.Equal(0, snapshot.VisualPalette);
        Assert.Equal(0, snapshot.VisualColorIndex);
        Assert.Equal(0, snapshot.PriorityValue);
        Assert.Equal(252, snapshot.ControlValue);
        Assert.Equal(0, snapshot.PatternNumber);
        Assert.Equal(0, snapshot.PatternSize);
        Assert.Equal(4424, snapshot.PenX);
        Assert.Equal(4020, snapshot.PenY);
        Assert.Equal(PicStateFlags.None, snapshot.Flags);

        var expectedCounts = new Dictionary<PicOpcode, int>
        {
            [PicOpcode.SetPattern] = 2,
            [PicOpcode.SetControl] = 1,
            [PicOpcode.DisableControl] = 2,
            [PicOpcode.DisablePriority] = 1,
            [PicOpcode.DisableVisual] = 1,
            [PicOpcode.FloodFill] = 1,
            [PicOpcode.RelativeMediumPatterns] = 2,
            [PicOpcode.ExtendedFunction] = 2
        };

        Assert.Equal(expectedCounts, counts);
    }

    [Fact]
    public void Sci11_TemplatePicSnapshotMatches()
    {
        var folder = RepoPaths.GetTemplateGame("SCI1.1");
        var catalog = _discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == ResourceType.Pic);
        var payload = LoadPicPayload(folder, descriptor, catalog.Version);
        var result = PicParser.Parse(payload, catalog.Version);

        var snapshot = result.FinalState;
        var counts = PicParser.AggregateOpcodeCounts(result.Commands);

        Assert.Equal(0, snapshot.VisualPalette);
        Assert.Equal(216, snapshot.VisualColorIndex);
        Assert.Equal(0, snapshot.PriorityValue);
        Assert.Equal(0, snapshot.ControlValue);
        Assert.Equal(0, snapshot.PatternNumber);
        Assert.Equal(0, snapshot.PatternSize);
        Assert.Equal(114, snapshot.PenX);
        Assert.Equal(102, snapshot.PenY);
        Assert.Equal(PicStateFlags.VisualEnabled | PicStateFlags.PriorityEnabled | PicStateFlags.ControlEnabled, snapshot.Flags);

        var expectedCounts = new Dictionary<PicOpcode, int>
        {
            [(PicOpcode)0x26] = 1,
            [(PicOpcode)0x46] = 2,
            [PicOpcode.SetVisualColor] = 1,
            [PicOpcode.FloodFill] = 2,
            [PicOpcode.AbsolutePatterns] = 1
        };

        Assert.Equal(expectedCounts, counts);
    }

    private byte[] LoadPicPayload(string folder, ResourceDescriptor descriptor, SCIVersion version)
    {
        var package = _reader.Read(folder, descriptor, version);
        if (package.Header.CompressionMethod == 0)
        {
            var length = package.Header.DecompressedLength > 0
                ? package.Header.DecompressedLength
                : package.Body.Length;
            var copy = new byte[length];
            Array.Copy(package.Body, copy, length);
            return copy;
        }

        return _registry.Decompress(package.Body, package.Header.CompressionMethod, package.Header.DecompressedLength);
    }
}
