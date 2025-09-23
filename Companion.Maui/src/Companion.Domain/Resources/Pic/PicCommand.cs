namespace Companion.Domain.Resources.Pic;

public abstract record PicCommand(PicOpcode Opcode)
{
    public sealed record SetPalette(byte PaletteIndex, byte[] Colors) : PicCommand(PicOpcode.SetPalette);

    public sealed record RelativeLine(IReadOnlyList<(int dx, int dy)> Segments, byte ColorIndex) : PicCommand(PicOpcode.RelativeDraw);

    public sealed record FloodFill(byte ColorIndex, ushort X, ushort Y) : PicCommand(PicOpcode.FloodFill);

    public sealed record Unknown(PicOpcode RawOpcode, byte[] Data) : PicCommand(RawOpcode);
}
