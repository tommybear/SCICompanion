using System;
using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources.Pic;

internal sealed class PicStateMachine
{
    private const int PaletteBankCount = 4;
    private const int PaletteSize = 40;
    private const int PriorityBandCount = 14;

    private static readonly byte[] DefaultEgaPalette =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x88,
        0x88, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x88,
        0x88, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
        0x08, 0x91, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x88
    };

    private readonly SCIVersion _version;
    private readonly byte[][] _paletteBanks;
    private readonly bool[] _paletteLocks;
    private readonly byte[] _vgaPalette;
    private readonly ushort[] _priorityBands;

    public PicStateMachine(SCIVersion version)
    {
        _version = version;
        _paletteBanks = new byte[PaletteBankCount][];
        for (var i = 0; i < PaletteBankCount; i++)
        {
            _paletteBanks[i] = new byte[PaletteSize];
            SeedDefaultPalette(_paletteBanks[i]);
        }
        _paletteLocks = new bool[PaletteSize];
        _vgaPalette = new byte[256 * 3];
        _priorityBands = new ushort[PriorityBandCount];
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
            VgaPalette: new byte[_vgaPalette.Length],
            PriorityBands: (ushort[])_priorityBands.Clone());
    }

    public PicStateSnapshot Snapshot { get; private set; }

    public PicStateSnapshot GetSnapshot()
    {
        return Snapshot with
        {
            PaletteLocks = (bool[])Snapshot.PaletteLocks.Clone(),
            PaletteBanks = ClonePaletteBanks(),
            VgaPalette = (byte[])Snapshot.VgaPalette.Clone(),
            PriorityBands = (ushort[])Snapshot.PriorityBands.Clone()
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

    public void UpdatePatternNumber(byte number)
    {
        Snapshot = Snapshot with
        {
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
            case PicExtendedOpcode.SetPalette when _version <= SCIVersion.SCI0:
                {
                    var bank = Snapshot.PaletteToDraw % PaletteBankCount;
                    var target = _paletteBanks[bank];
                    var count = Math.Min(target.Length, data.Length);
                    data[..count].CopyTo(target);
                }
                break;
            case PicExtendedOpcode.SetPaletteEntry when _version <= SCIVersion.SCI0:
                for (var i = 0; i + 1 < data.Length; i += 2)
                {
                    var palette = (data[i] / PaletteSize) % PaletteBankCount;
                    var offset = data[i] % PaletteSize;
                    _paletteBanks[palette][offset] = data[i + 1];
                }
                break;
            case PicExtendedOpcode.SetPalette when _version > SCIVersion.SCI0:
            case PicExtendedOpcode.Sci1SetPalette when _version > SCIVersion.SCI0:
                ApplyVgaPalette(data);
                break;
            case PicExtendedOpcode.SetPaletteEntry when _version > SCIVersion.SCI0:
                ApplyVgaPaletteEntry(data);
                break;
            case PicExtendedOpcode.Sci1SetPriorityBands:
                ApplyPriorityBands(data);
                break;
            case PicExtendedOpcode.Sci1DrawBitmap:
                // TODO: Support embedded VGA bitmap drawing.
                break;
            default:
                break;
        }

        Snapshot = Snapshot with
        {
            PaletteBanks = ClonePaletteBanks(),
            VgaPalette = (byte[])_vgaPalette.Clone(),
            PriorityBands = (ushort[])_priorityBands.Clone()
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

    private static void SeedDefaultPalette(Span<byte> target)
    {
        var count = Math.Min(DefaultEgaPalette.Length, target.Length);
        DefaultEgaPalette.AsSpan(0, count).CopyTo(target);
    }

    private void ApplyPriorityBands(ReadOnlySpan<byte> data)
    {
        if (data.Length >= _priorityBands.Length * 2)
        {
            for (var i = 0; i < _priorityBands.Length; i++)
            {
                var low = data[i * 2];
                var high = data[i * 2 + 1];
                _priorityBands[i] = (ushort)(low | (high << 8));
            }
            return;
        }

        var count = Math.Min(_priorityBands.Length, data.Length);
        for (var i = 0; i < count; i++)
        {
            _priorityBands[i] = data[i];
        }
    }

    private void ApplyVgaPalette(ReadOnlySpan<byte> data)
    {
        var maxEntries = Math.Min(256, data.Length / 3);
        for (var i = 0; i < maxEntries; i++)
        {
            var sourceIndex = i * 3;
            var targetIndex = i * 3;
            _vgaPalette[targetIndex] = data[sourceIndex];
            _vgaPalette[targetIndex + 1] = data[sourceIndex + 1];
            _vgaPalette[targetIndex + 2] = data[sourceIndex + 2];
        }
    }

    private void ApplyVgaPaletteEntry(ReadOnlySpan<byte> data)
    {
        var cursor = 0;
        while (cursor < data.Length)
        {
            var index = (int)data[cursor++];
            if (index >= 256)
            {
                continue;
            }

            var remaining = data.Length - cursor;
            var target = index * 3;

            if (remaining >= 3)
            {
                _vgaPalette[target] = data[cursor];
                _vgaPalette[target + 1] = data[cursor + 1];
                _vgaPalette[target + 2] = data[cursor + 2];
                cursor += 3;
            }
            else if (remaining >= 1)
            {
                // Treat single component payloads as grayscale for compatibility with
                // incomplete palette data in early assets.
                var value = data[cursor++];
                _vgaPalette[target] = value;
                _vgaPalette[target + 1] = value;
                _vgaPalette[target + 2] = value;
            }
        }
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
    byte[] VgaPalette,
    ushort[] PriorityBands)
{
    public PicStateSnapshot WithFlags(PicStateFlags flags) => this with { Flags = flags };
}

public sealed record PicParseResult(
    IReadOnlyList<PicCommand> Commands,
    IReadOnlyList<PicStateSnapshot> StateTimeline,
    PicStateSnapshot FinalState);
