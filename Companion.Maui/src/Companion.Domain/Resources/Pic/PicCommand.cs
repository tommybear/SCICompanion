using System.Collections.Generic;

namespace Companion.Domain.Resources.Pic;

public abstract record PicCommand(PicOpcode Opcode)
{
    public sealed record SetVisual(byte Palette, byte ColorIndex) : PicCommand(PicOpcode.SetVisualColor);

    public sealed record DisableVisual() : PicCommand(PicOpcode.DisableVisual);

    public sealed record SetPriority(byte Value) : PicCommand(PicOpcode.SetPriority);

    public sealed record DisablePriority() : PicCommand(PicOpcode.DisablePriority);

    public sealed record SetControl(byte Value) : PicCommand(PicOpcode.SetControl);

    public sealed record DisableControl() : PicCommand(PicOpcode.DisableControl);

    public sealed record SetPattern(byte PatternNumber, byte PatternSize, bool IsRectangle, bool UseBrush) : PicCommand(PicOpcode.SetPattern);

    public sealed record RelativeLine(PicOpcode LineOpcode, byte ColorIndex, IReadOnlyList<(int dx, int dy)> Segments, int StartX, int StartY, int EndX, int EndY)
        : PicCommand(LineOpcode);

    public sealed record FloodFill(byte ColorIndex, ushort X, ushort Y) : PicCommand(PicOpcode.FloodFill);

    public sealed record Extended(PicExtendedOpcode ExtendedOpcode, byte[] Data) : PicCommand(PicOpcode.ExtendedFunction);

    public sealed record Unknown(PicOpcode RawOpcode, byte[] Data) : PicCommand(RawOpcode);
}
