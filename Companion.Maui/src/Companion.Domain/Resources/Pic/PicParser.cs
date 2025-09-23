using System.Buffers.Binary;
using System.Collections.Generic;

namespace Companion.Domain.Resources.Pic;

public static class PicParser
{
    public static IReadOnlyList<PicCommand> Parse(byte[] payload)
    {
        var commands = new List<PicCommand>();
        var index = 0;
        while (index < payload.Length)
        {
            var opcode = payload[index++];
            if (opcode >= (byte)PicOpcode.Terminator)
            {
                break;
            }

            switch ((PicOpcode)opcode)
            {
                case PicOpcode.SetPalette:
                    commands.Add(ParsePalette(payload, ref index));
                    break;
                case PicOpcode.RelativeDraw:
                    commands.Add(ParseRelativeLines(payload, ref index));
                    break;
                case PicOpcode.FloodFill:
                    commands.Add(ParseFloodFill(payload, ref index));
                    break;
                default:
                    commands.Add(new PicCommand.Unknown((PicOpcode)opcode, ReadUnknown(payload, ref index)));
                    break;
            }
        }

        return commands;
    }

    public static IReadOnlyDictionary<PicOpcode, int> AggregateOpcodeCounts(IEnumerable<PicCommand> commands)
    {
        var counts = new Dictionary<PicOpcode, int>();
        foreach (var command in commands)
        {
            counts[command.Opcode] = counts.TryGetValue(command.Opcode, out var existing)
                ? existing + 1
                : 1;
        }
        return counts;
    }

    private static PicCommand SetPaletteFallback => new PicCommand.SetPalette(0, Array.Empty<byte>());
    
    private static PicCommand ParsePalette(byte[] payload, ref int index)
    {
        if (index + 1 >= payload.Length)
        {
            return SetPaletteFallback;
        }

        byte paletteIndex = payload[index++];
        byte colorCount = payload[index++];
        int byteCount = colorCount * 3;
        if (index + byteCount > payload.Length)
        {
            byteCount = Math.Max(0, payload.Length - index);
        }

        var colors = new byte[byteCount];
        Array.Copy(payload, index, colors, 0, byteCount);
        index += byteCount;
        return new PicCommand.SetPalette(paletteIndex, colors);
    }

    private static PicCommand ParseRelativeLines(byte[] payload, ref int index)
    {
        if (index >= payload.Length)
        {
            return new PicCommand.RelativeLine(Array.Empty<(int, int)>(), 0);
        }

        byte color = payload[index++];
        var segments = new List<(int dx, int dy)>();
        while (index < payload.Length)
        {
            var value = payload[index];
            if (value >= (byte)PicOpcode.Terminator)
            {
                break;
            }
            index++;
            int dx = ((value >> 4) & 0x7) * (value.HasBit(0x80) ? -1 : 1);
            int dy = (value & 0x7) * (value.HasBit(0x08) ? -1 : 1);
            segments.Add((dx, dy));
        }

        return new PicCommand.RelativeLine(segments, color);
    }

    private static PicCommand ParseFloodFill(byte[] payload, ref int index)
    {
        if (index + 3 > payload.Length)
        {
            return new PicCommand.FloodFill(0, 0, 0);
        }

        byte color = payload[index++];
        ushort x = payload[index++];
        ushort y = payload[index++];
        return new PicCommand.FloodFill(color, x, y);
    }

    private static byte[] ReadUnknown(byte[] payload, ref int index)
    {
        var start = index;
        while (index < payload.Length && payload[index] < (byte)PicOpcode.Terminator)
        {
            index++;
        }

        var length = index - start;
        var data = new byte[length];
        Array.Copy(payload, start, data, 0, length);
        return data;
    }

    private static bool HasBit(this byte value, int mask) => (value & mask) != 0;
}
