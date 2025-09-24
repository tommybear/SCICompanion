using System;
using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources.Pic;

internal sealed class PicStateMachine
{
    private const int PaletteBankCount = 4;
    private const int PaletteSize = 40;

    private readonly SCIVersion _version;
    private readonly byte[][] _paletteBanks;
    private readonly bool[] _paletteLocks;
    private readonly byte[] _vgaPalette;

    public PicStateMachine(SCIVersion version)
    {
        _version = version;
        _paletteBanks = new byte[PaletteBankCount][];
        for (var i = 0; i < PaletteBankCount; i++)
        {
            _paletteBanks[i] = new byte[PaletteSize];
        }
        _paletteLocks = new bool[PaletteSize];
        _vgaPalette = new byte[256];
        Snapshot = new PicStateSnapshot(
            Flags: PicStateFlags.VisualEnabled | PicStateFlags.PriorityEnabled | PicStateFlags.ControlEnabled,
            VisualPalette: 0,
            VisualColorIndex: 0,
            PriorityValue: 0,
            ControlValue: 0,
            PatternNumber: 0,
            PatternSize: 0,
            PenX: 0,
            PenY: 0,
            PaletteToDraw: 0,
            PaletteLocks: new bool[PaletteSize],
            PaletteBanks: ClonePaletteBanks(),
            VgaPalette: new byte[_vgaPalette.Length]);
    }

    public PicStateSnapshot Snapshot { get; private set; }

    public PicStateSnapshot GetSnapshot()
    {
        return Snapshot with
        {
            PaletteLocks = (bool[])Snapshot.PaletteLocks.Clone(),
            PaletteBanks = ClonePaletteBanks(),
            VgaPalette = (byte[])Snapshot.VgaPalette.Clone()
        };
    }

    public void SetVisual(byte colorCode)
    {
        byte palette;
        byte index;
        if (_version <= SCIVersion.SCI0)
        {
            palette = (byte)(colorCode / PaletteSize);
            index = (byte)(colorCode % PaletteSize);
        }
        else
        {
            palette = Snapshot.PaletteToDraw;
            index = colorCode;
        }

        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags | PicStateFlags.VisualEnabled,
            VisualPalette = palette,
            VisualColorIndex = index
        };
    }

    public void DisableVisual()
    {
        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags & ~PicStateFlags.VisualEnabled
        };
    }

    public void SetPriority(byte value)
    {
        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags | PicStateFlags.PriorityEnabled,
            PriorityValue = (byte)(value & 0x0F)
        };
    }

    public void DisablePriority()
    {
        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags & ~PicStateFlags.PriorityEnabled
        };
    }

    public void SetControl(byte value)
    {
        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags | PicStateFlags.ControlEnabled,
            ControlValue = value
        };
    }

    public void DisableControl()
    {
        Snapshot = Snapshot with
        {
            Flags = Snapshot.Flags & ~PicStateFlags.ControlEnabled
        };
    }

    public void SetPattern(byte patternCode)
    {
        var trimmed = (byte)(patternCode & 0x37);
        var size = (byte)(trimmed & 0x07);
        var number = (byte)((trimmed >> 3) & 0x07);
        var useBrush = (trimmed & 0x08) != 0;
        var rectangle = (trimmed & 0x20) != 0;

        var flags = Snapshot.Flags;
        flags = useBrush ? flags | PicStateFlags.PatternUsesBrush : flags & ~PicStateFlags.PatternUsesBrush;
        flags = rectangle ? flags | PicStateFlags.PatternIsRectangle : flags & ~PicStateFlags.PatternIsRectangle;

        Snapshot = Snapshot with
        {
            Flags = flags,
            PatternSize = size,
            PatternNumber = number
        };
    }

    public void UpdatePen(int x, int y)
    {
        Snapshot = Snapshot with
        {
            PenX = x,
            PenY = y
        };
    }

    public void OffsetPen(int dx, int dy)
    {
        Snapshot = Snapshot with
        {
            PenX = Snapshot.PenX + dx,
            PenY = Snapshot.PenY + dy
        };
    }

    public void SetPaletteToDraw(byte palette)
    {
        Snapshot = Snapshot with
        {
            PaletteToDraw = palette
        };
    }

    public void ApplyExtended(PicExtendedOpcode opcode, ReadOnlySpan<byte> data)
    {
        switch (opcode)
        {
            case PicExtendedOpcode.SetPalette:
                if (_version <= SCIVersion.SCI0)
                {
                    var bank = Snapshot.PaletteToDraw % PaletteBankCount;
                    var target = _paletteBanks[bank];
                    var count = Math.Min(target.Length, data.Length);
                    data[..count].CopyTo(target);
                }
                else
                {
                    var count = Math.Min(_vgaPalette.Length, data.Length);
                    data[..count].CopyTo(_vgaPalette);
                }
                break;
            case PicExtendedOpcode.SetPaletteEntry:
                if (_version <= SCIVersion.SCI0)
                {
                    for (var i = 0; i + 1 < data.Length; i += 2)
                    {
                        var palette = (data[i] / PaletteSize) % PaletteBankCount;
                        var offset = data[i] % PaletteSize;
                        _paletteBanks[palette][offset] = data[i + 1];
                    }
                }
                else
                {
                    for (var i = 0; i + 1 < data.Length; i += 2)
                    {
                        var index = data[i];
                        _vgaPalette[index] = data[i + 1];
                    }
                }
                break;
            default:
                break;
        }

        Snapshot = Snapshot with
        {
            PaletteBanks = ClonePaletteBanks(),
            VgaPalette = (byte[])_vgaPalette.Clone()
        };
    }

    private byte[][] ClonePaletteBanks()
    {
        var clone = new byte[_paletteBanks.Length][];
        for (var i = 0; i < _paletteBanks.Length; i++)
        {
            clone[i] = (byte[])_paletteBanks[i].Clone();
        }
        return clone;
    }
}

public sealed record PicStateSnapshot(
    PicStateFlags Flags,
    byte VisualPalette,
    byte VisualColorIndex,
    byte PriorityValue,
    byte ControlValue,
    byte PatternNumber,
    byte PatternSize,
    int PenX,
    int PenY,
    byte PaletteToDraw,
    bool[] PaletteLocks,
    byte[][] PaletteBanks,
    byte[] VgaPalette)
{
    public PicStateSnapshot WithFlags(PicStateFlags flags) => this with { Flags = flags };
}

public sealed record PicParseResult(
    IReadOnlyList<PicCommand> Commands,
    IReadOnlyList<PicStateSnapshot> StateTimeline,
    PicStateSnapshot FinalState);
