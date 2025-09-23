namespace Companion.Domain.Resources;

public sealed record PaletteEntry(byte Red, byte Green, byte Blue)
{
    public (byte Red, byte Green, byte Blue) ToTuple() => (Red, Green, Blue);
}
