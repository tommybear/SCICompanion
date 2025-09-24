using System;

namespace Companion.Domain.Resources.Pic;

public enum PicOpcode : byte
{
    SetVisualColor = 0xF0,
    DisableVisual = 0xF1,
    SetPriority = 0xF2,
    DisablePriority = 0xF3,
    RelativePatterns = 0xF4,
    RelativeMediumLines = 0xF5,
    RelativeLongLines = 0xF6,
    RelativeShortLines = 0xF7,
    FloodFill = 0xF8,
    SetPattern = 0xF9,
    AbsolutePatterns = 0xFA,
    SetControl = 0xFB,
    DisableControl = 0xFC,
    RelativeMediumPatterns = 0xFD,
    ExtendedFunction = 0xFE,
    End = 0xFF
}

[Flags]
public enum PicStateFlags
{
    None = 0,
    VisualEnabled = 1 << 0,
    PriorityEnabled = 1 << 1,
    ControlEnabled = 1 << 2,
    PatternUsesBrush = 1 << 3,
    PatternIsRectangle = 1 << 4
}

public enum PicExtendedOpcode : byte
{
    SetPaletteEntry = 0x00,
    SetPalette = 0x01,
    Sci1DrawBitmap = 0x01,
    Sci1SetPalette = 0x02,
    Sci1SetPriorityBands = 0x04,
    Unknown = 0xFF
}

internal static class PicOpcodeExtensions
{
    public static bool IsTerminator(this byte value) => value >= (byte)PicOpcode.SetVisualColor && value <= (byte)PicOpcode.End;
}
