using System;
using System.Buffers;
using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources.Pic;

internal sealed class PicEncoder
{
    public byte[] Encode(PicDocument document, SCIVersion version, byte[] originalPayload)
    {
        if (document is null)
        {
            throw new ArgumentNullException(nameof(document));
        }

        if (originalPayload is null)
        {
            throw new ArgumentNullException(nameof(originalPayload));
        }

        try
        {
            var writer = new ArrayBufferWriter<byte>(originalPayload.Length + 16);
            var stateMachine = new PicStateMachine(version);

            foreach (var command in document.Commands)
            {
                var stateBefore = stateMachine.GetSnapshot();
                WriteCommand(writer, command, version, stateBefore);
                UpdateState(stateMachine, command, version);
            }

            WriteByte(writer, (byte)PicOpcode.End);
            return writer.WrittenSpan.ToArray();
        }
        catch (NotSupportedException)
        {
            return CloneOriginal(originalPayload);
        }
        catch (InvalidOperationException)
        {
            return CloneOriginal(originalPayload);
        }
    }

    private static void WriteCommand(IBufferWriter<byte> writer, PicCommand command, SCIVersion version, PicStateSnapshot stateBefore)
    {
        switch (command)
        {
            case PicCommand.SetVisual set:
                WriteByte(writer, (byte)PicOpcode.SetVisualColor);
                WriteByte(writer, GetVisualColorCode(set, version));
                break;
            case PicCommand.DisableVisual:
                WriteByte(writer, (byte)PicOpcode.DisableVisual);
                break;
            case PicCommand.SetPriority set:
                WriteByte(writer, (byte)PicOpcode.SetPriority);
                WriteByte(writer, (byte)(set.Value & 0x0F));
                break;
            case PicCommand.DisablePriority:
                WriteByte(writer, (byte)PicOpcode.DisablePriority);
                break;
            case PicCommand.SetControl set:
                WriteByte(writer, (byte)PicOpcode.SetControl);
                WriteByte(writer, set.Value);
                break;
            case PicCommand.DisableControl:
                WriteByte(writer, (byte)PicOpcode.DisableControl);
                break;
            case PicCommand.SetPattern pattern:
                WriteByte(writer, (byte)PicOpcode.SetPattern);
                WriteByte(writer, ComposePatternByte(pattern));
                break;
            case PicCommand.RelativeLine line:
                WriteRelativeLine(writer, line);
                break;
            case PicCommand.PatternDraw pattern:
                WritePatternDraw(writer, pattern, stateBefore);
                break;
            case PicCommand.FloodFill fill:
                WriteByte(writer, (byte)PicOpcode.FloodFill);
                WriteByte(writer, fill.ColorIndex);
                WriteByte(writer, (byte)(fill.X & 0xFF));
                WriteByte(writer, (byte)(fill.Y & 0xFF));
                break;
            case PicCommand.Extended extended:
                WriteByte(writer, (byte)PicOpcode.ExtendedFunction);
                WriteByte(writer, (byte)extended.ExtendedOpcode);
                if (extended.Data.Length > 0)
                {
                    WriteBytes(writer, extended.Data);
                }
                break;
            case PicCommand.Unknown unknown:
                throw new NotSupportedException($"Cannot encode unknown opcode {unknown.RawOpcode}.");
            default:
                throw new NotSupportedException($"Unsupported PIC command type '{command.GetType().Name}'.");
        }
    }

    private static void WriteRelativeLine(IBufferWriter<byte> writer, PicCommand.RelativeLine line)
    {
        switch (line.LineOpcode)
        {
            case PicOpcode.RelativeShortLines:
                WriteByte(writer, (byte)PicOpcode.RelativeShortLines);
                WriteByte(writer, line.ColorIndex);
                WriteRelativeShortSegments(writer, line.Segments);
                break;
            case PicOpcode.RelativeMediumLines:
                WriteByte(writer, (byte)PicOpcode.RelativeMediumLines);
                WriteAbsoluteCoordinate(writer, line.StartX, line.StartY);
                WriteRelativeMediumSegments(writer, line.Segments, line.StartX, line.StartY);
                break;
            case PicOpcode.RelativeLongLines:
                WriteByte(writer, (byte)PicOpcode.RelativeLongLines);
                WriteAbsoluteCoordinate(writer, line.StartX, line.StartY);
                WriteRelativeLongSegments(writer, line.Segments, line.StartX, line.StartY);
                break;
            default:
                throw new NotSupportedException($"Unsupported line opcode '{line.LineOpcode}'.");
        }
    }

    private static void WritePatternDraw(IBufferWriter<byte> writer, PicCommand.PatternDraw pattern, PicStateSnapshot stateBefore)
    {
        WriteByte(writer, (byte)pattern.PatternOpcode);

        var instances = pattern.Instances;
        if (instances.Count == 0)
        {
            return;
        }

        switch (pattern.PatternOpcode)
        {
            case PicOpcode.RelativePatterns:
            case PicOpcode.RelativeMediumPatterns:
                WriteRelativePattern(writer, pattern, stateBefore);
                break;
            case PicOpcode.AbsolutePatterns:
                WriteAbsolutePattern(writer, pattern);
                break;
            default:
                throw new NotSupportedException($"Unsupported pattern opcode '{pattern.PatternOpcode}'.");
        }
    }

    private static void WriteRelativeShortSegments(IBufferWriter<byte> writer, IReadOnlyList<(int dx, int dy)> segments)
    {
        foreach (var (dx, dy) in segments)
        {
            if (dx < -7 || dx > 7)
            {
                throw new NotSupportedException("Relative short line X delta out of range.");
            }
            if (dy < -7 || dy > 7)
            {
                throw new NotSupportedException("Relative short line Y delta out of range.");
            }

            byte value = (byte)((Math.Abs(dx) & 0x07) << 4 | (Math.Abs(dy) & 0x07));
            if (dx < 0)
            {
                value |= 0x80;
            }
            if (dy < 0)
            {
                value |= 0x08;
            }
            WriteByte(writer, value);
        }
    }

    private static void WriteRelativeMediumSegments(IBufferWriter<byte> writer, IReadOnlyList<(int dx, int dy)> segments, int startX, int startY)
    {
        var currentX = startX;
        var currentY = startY;

        foreach (var (dx, dy) in segments)
        {
            var nextX = currentX + dx;
            var nextY = currentY + dy;

            var deltaY = nextY - currentY;
            if (deltaY < -0x7F || deltaY > 0x7F)
            {
                throw new NotSupportedException("Relative medium line Y delta out of range.");
            }
            byte yByte = deltaY < 0 ? (byte)(0x80 | (-deltaY)) : (byte)deltaY;

            var deltaX = nextX - currentX;
            if (deltaX < -128 || deltaX > 127)
            {
                throw new NotSupportedException("Relative medium line X delta out of range.");
            }
            byte xByte = unchecked((byte)(sbyte)deltaX);

            WriteByte(writer, yByte);
            WriteByte(writer, xByte);

            currentX = nextX;
            currentY = nextY;
        }
    }

    private static void WriteRelativeLongSegments(IBufferWriter<byte> writer, IReadOnlyList<(int dx, int dy)> segments, int startX, int startY)
    {
        var currentX = startX;
        var currentY = startY;

        foreach (var (dx, dy) in segments)
        {
            currentX += dx;
            currentY += dy;
            WriteAbsoluteCoordinate(writer, currentX, currentY);
        }
    }

    private static void WriteRelativePattern(IBufferWriter<byte> writer, PicCommand.PatternDraw pattern, PicStateSnapshot stateBefore)
    {
        var instances = pattern.Instances;
        var first = instances[0];
        var currentPattern = stateBefore.PatternNumber;

        if (pattern.UseBrush && (instances[0].PatternNumber != currentPattern))
        {
            WriteBrushPattern(writer, instances[0].PatternNumber);
            currentPattern = instances[0].PatternNumber;
        }

        WriteAbsoluteCoordinate(writer, first.X, first.Y);

        var currentX = first.X;
        var currentY = first.Y;

        for (var i = 1; i < instances.Count; i++)
        {
            var instance = instances[i];

            if (pattern.UseBrush)
            {
                WriteBrushPattern(writer, instance.PatternNumber);
                currentPattern = instance.PatternNumber;
            }

            var deltaY = instance.Y - currentY;
            if (deltaY < -0x7F || deltaY > 0x7F)
            {
                throw new NotSupportedException("Relative pattern Y delta out of range.");
            }
            var yByte = deltaY < 0 ? (byte)(0x80 | (-deltaY)) : (byte)deltaY;

            var deltaX = instance.X - currentX;
            if (deltaX < -128 || deltaX > 127)
            {
                throw new NotSupportedException("Relative pattern X delta out of range.");
            }
            var xByte = unchecked((byte)(sbyte)deltaX);

            WriteByte(writer, yByte);
            WriteByte(writer, xByte);

            currentX = instance.X;
            currentY = instance.Y;
        }
    }

    private static void WriteAbsolutePattern(IBufferWriter<byte> writer, PicCommand.PatternDraw pattern)
    {
        foreach (var instance in pattern.Instances)
        {
            WriteAbsoluteCoordinate(writer, instance.X, instance.Y);
        }
    }

    private static void WriteAbsoluteCoordinate(IBufferWriter<byte> writer, int x, int y)
    {
        if (x < 0 || x > 0x0FFF || y < 0 || y > 0x0FFF)
        {
            throw new NotSupportedException("Absolute coordinate out of range.");
        }

        byte prefix = (byte)(((x >> 4) & 0xF0) | ((y >> 8) & 0x0F));
        WriteByte(writer, prefix);
        WriteByte(writer, (byte)(x & 0xFF));
        WriteByte(writer, (byte)(y & 0xFF));
    }

    private static void WriteBrushPattern(IBufferWriter<byte> writer, int patternNumber)
    {
        if (patternNumber < 0 || patternNumber > 0x7F)
        {
            throw new NotSupportedException("Pattern number out of range.");
        }

        var value = (byte)((patternNumber & 0x7F) << 1);
        WriteByte(writer, value);
    }

    private static byte ComposePatternByte(PicCommand.SetPattern pattern)
    {
        byte value = (byte)(((pattern.PatternNumber & 0x07) << 3) | (pattern.PatternSize & 0x07));
        if (pattern.UseBrush)
        {
            value |= 0x08;
        }
        if (pattern.IsRectangle)
        {
            value |= 0x20;
        }
        return value;
    }

    private static byte GetVisualColorCode(PicCommand.SetVisual setVisual, SCIVersion version)
    {
        return version <= SCIVersion.SCI0
            ? (byte)(setVisual.Palette * 40 + setVisual.ColorIndex)
            : setVisual.ColorIndex;
    }

    private static void UpdateState(PicStateMachine stateMachine, PicCommand command, SCIVersion version)
    {
        switch (command)
        {
            case PicCommand.SetVisual set:
                stateMachine.SetVisual(GetVisualColorCode(set, version));
                break;
            case PicCommand.DisableVisual:
                stateMachine.DisableVisual();
                break;
            case PicCommand.SetPriority set:
                stateMachine.SetPriority(set.Value);
                break;
            case PicCommand.DisablePriority:
                stateMachine.DisablePriority();
                break;
            case PicCommand.SetControl set:
                stateMachine.SetControl(set.Value);
                break;
            case PicCommand.DisableControl:
                stateMachine.DisableControl();
                break;
            case PicCommand.SetPattern pattern:
                stateMachine.SetPattern(ComposePatternByte(pattern));
                break;
            case PicCommand.RelativeLine line:
                stateMachine.UpdatePen(line.EndX, line.EndY);
                break;
            case PicCommand.PatternDraw pattern:
                if (pattern.Instances.Count > 0)
                {
                    var last = pattern.Instances[^1];
                    stateMachine.UpdatePen(last.X, last.Y);
                    if (pattern.UseBrush)
                    {
                        stateMachine.UpdatePatternNumber(last.PatternNumber);
                    }
                }
                break;
            case PicCommand.FloodFill fill:
                stateMachine.UpdatePen(fill.X, fill.Y);
                break;
            case PicCommand.Extended extended:
                stateMachine.ApplyExtended(extended.ExtendedOpcode, extended.Data);
                break;
        }
    }

    private static void WriteByte(IBufferWriter<byte> writer, byte value)
    {
        var span = writer.GetSpan(1);
        span[0] = value;
        writer.Advance(1);
    }

    private static void WriteBytes(IBufferWriter<byte> writer, ReadOnlySpan<byte> data)
    {
        if (data.Length == 0)
        {
            return;
        }

        var span = writer.GetSpan(data.Length);
        data.CopyTo(span);
        writer.Advance(data.Length);
    }

    private static byte[] CloneOriginal(byte[] original)
    {
        var copy = new byte[original.Length];
        Array.Copy(original, copy, copy.Length);
        return copy;
    }
}
