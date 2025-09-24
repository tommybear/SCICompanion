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
}
