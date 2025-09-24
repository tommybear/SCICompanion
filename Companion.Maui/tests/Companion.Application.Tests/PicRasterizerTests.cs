using Companion.Domain.Resources.Pic;
using Companion.Domain.Projects;
using Xunit;

namespace Companion.Application.Tests;

public class PicRasterizerTests
{
    [Fact]
    public void Render_DrawsLineWithCurrentState()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetVisualColor, 0x04, // palette 0, index 4
            (byte)PicOpcode.SetPriority, 0x06,
            (byte)PicOpcode.SetControl, 0x02,
            (byte)PicOpcode.RelativeShortLines, 0x04, 0x11,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI0);
        var document = PicRasterizer.Render(parseResult, SCIVersion.SCI0);

        Assert.Equal(320, document.Width);
        Assert.Equal(190, document.Height);

        // After a single relative step of (1,1) from origin a line should connect (0,0) -> (1,1).
        var visual = document.VisualPlane;
        var priority = document.PriorityPlane;
        var control = document.ControlPlane;

        int index00 = 0;
        int index11 = 1 + 320; // (1,1)

        Assert.Equal(4, visual[index00]);
        Assert.Equal(4, visual[index11]);
        Assert.Equal((byte)0x06, priority[index11]);
        Assert.Equal((byte)0x02, control[index11]);
    }

    [Fact]
    public void RenderHonorsDisableVisual()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.DisableVisual,
            (byte)PicOpcode.RelativeShortLines, 0x05, 0x11,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI0);
        var document = PicRasterizer.Render(parseResult, SCIVersion.SCI0);

        var visual = document.VisualPlane;
        Assert.All(visual, b => Assert.Equal((byte)0, b));
    }

    [Fact]
    public void RenderSupportsMediumLines()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetVisualColor, 0x03,
            (byte)PicOpcode.RelativeMediumLines,
            0x00, 0x0A, 0x0A,
            0x03, 0x04,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI0);
        var document = PicRasterizer.Render(parseResult, SCIVersion.SCI0);

        var indexStart = 10 + 10 * 320;
        var indexEnd = 14 + 13 * 320;

        Assert.Equal(3, document.VisualPlane[indexStart]);
        Assert.Equal(3, document.VisualPlane[indexEnd]);
    }

    [Fact]
    public void RenderSupportsLongLines()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetVisualColor, 0x05,
            (byte)PicOpcode.RelativeLongLines,
            0x00, 0x0A, 0x0A,
            0x00, 0x0E, 0x0D,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI0);
        var document = PicRasterizer.Render(parseResult, SCIVersion.SCI0);

        var start = 10 + 10 * 320;
        var end = 14 + 13 * 320;

        Assert.Equal((byte)5, document.VisualPlane[start]);
        Assert.Equal((byte)5, document.VisualPlane[end]);
    }

    [Fact]
    public void ExtendedPriorityBandsUpdateState()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.ExtendedFunction,
            (byte)PicExtendedOpcode.Sci1SetPriorityBands,
            0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00,
            0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00,
            0x09, 0x00, 0x0A, 0x00, 0x0B, 0x00, 0x0C, 0x00,
            0x0D, 0x00, 0x0E, 0x00,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI1);

        Assert.Equal(14, parseResult.FinalState.PriorityBands.Length);
        Assert.Equal((ushort)1, parseResult.FinalState.PriorityBands[0]);
        Assert.Equal((ushort)14, parseResult.FinalState.PriorityBands[^1]);
    }

    [Fact]
    public void RenderMarksPatternStamp()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetVisualColor, 0x02,
            (byte)PicOpcode.SetPattern, 0x01,
            (byte)PicOpcode.RelativePatterns,
            0x00, 0x05, 0x05,
            (byte)PicOpcode.End
        };

        var parseResult = PicParser.Parse(payload, SCIVersion.SCI0);
        var document = PicRasterizer.Render(parseResult, SCIVersion.SCI0);

        var visual = document.VisualPlane;
        var center = 5 + 5 * 320;
        Assert.Equal((byte)2, visual[center]);
        Assert.Equal((byte)2, visual[5 + 4 * 320]); // Up
        Assert.Equal((byte)2, visual[5 + 6 * 320]); // Down
        Assert.Equal((byte)2, visual[4 + 5 * 320]); // Left
        Assert.Equal((byte)2, visual[6 + 5 * 320]); // Right
        Assert.Equal((byte)0, visual[0]);
    }
}
