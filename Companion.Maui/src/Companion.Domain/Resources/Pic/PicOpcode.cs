namespace Companion.Domain.Resources.Pic;

public enum PicOpcode : byte
{
    End = 0x00,
    ChangePalette = 0x01,
    SetPriorityColor = 0x02,
    SetControlColor = 0x03,
    DisableVisual = 0x04,
    DisablePriority = 0x05,
    DisableControl = 0x06,
    SetPalette = 0x07,
    SetPriority = 0x08,
    SetControl = 0x09,
    RelativeDraw = 0x0A,
    Fill = 0x0B,
    AbsoluteLine = 0x0C,
    RelativePattern = 0x0D,
    FloodFill = 0x0E,
    XorPixel = 0x0F,
    Spare = 0x10,
    Terminator = 0xF0
}
