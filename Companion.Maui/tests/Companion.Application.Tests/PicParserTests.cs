using Companion.Domain.Resources.Pic;
using Companion.Domain.Projects;
using Xunit;

namespace Companion.Application.Tests;

public class PicParserTests
{
    [Fact]
    public void ParserTracksVisualPriorityAndControlState()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetVisualColor, 0x28,
            (byte)PicOpcode.SetPriority, 0x05,
            (byte)PicOpcode.SetControl, 0x03,
            (byte)PicOpcode.DisablePriority,
            (byte)PicOpcode.End
        };

        var result = PicParser.Parse(payload, SCIVersion.SCI0);
        var final = result.FinalState;

        Assert.True(final.Flags.HasFlag(PicStateFlags.VisualEnabled));
        Assert.False(final.Flags.HasFlag(PicStateFlags.PriorityEnabled));
        Assert.True(final.Flags.HasFlag(PicStateFlags.ControlEnabled));
        Assert.Equal((byte)(0x28 / 40), final.VisualPalette);
        Assert.Equal((byte)(0x28 % 40), final.VisualColorIndex);
        Assert.Equal((byte)0x03, final.ControlValue);
        Assert.Equal(4, result.StateTimeline.Count);
    }

    [Fact]
    public void ParserTracksPatternState()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.SetPattern, 0x29,
            (byte)PicOpcode.End
        };

        var result = PicParser.Parse(payload, SCIVersion.SCI0);
        var final = result.FinalState;

        Assert.True(final.Flags.HasFlag(PicStateFlags.PatternIsRectangle));
        Assert.False(final.Flags.HasFlag(PicStateFlags.PatternUsesBrush));
        Assert.Equal((byte)4, final.PatternNumber);
        Assert.Equal((byte)1, final.PatternSize);
    }

    [Fact]
    public void ParserUpdatesPenWithRelativeShortLines()
    {
        var payload = new byte[]
        {
            (byte)PicOpcode.RelativeShortLines, 0x02, 0x11, 0x12,
            (byte)PicOpcode.End
        };

        var result = PicParser.Parse(payload, SCIVersion.SCI0);
        var final = result.FinalState;

        Assert.Equal(2, final.PenX);
        Assert.Equal(3, final.PenY);
        Assert.Single(result.Commands);
        var command = Assert.IsType<PicCommand.RelativeLine>(result.Commands[0]);
        Assert.Equal((byte)0x02, command.ColorIndex);
        Assert.Equal((2, 3), (command.EndX, command.EndY));
    }
}
