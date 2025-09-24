using System;
using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources.Pic;

public static class PicParser
{
    public static PicParseResult Parse(byte[] payload, SCIVersion version)
    {
        var commands = new List<PicCommand>();
        var stateMachine = new PicStateMachine(version);
        var timeline = new List<PicStateSnapshot>();

        var index = 0;
        while (index < payload.Length)
        {
            var opcodeValue = payload[index++];
            var opcode = (PicOpcode)opcodeValue;

            if (opcode == PicOpcode.End)
            {
                break;
            }

            var command = ParseCommand(payload, ref index, opcode, stateMachine, version);
            commands.Add(command);
            timeline.Add(stateMachine.GetSnapshot());
        }

        byte[] trailing = Array.Empty<byte>();
        if (index < payload.Length)
        {
            trailing = new byte[payload.Length - index];
            Array.Copy(payload, index, trailing, 0, trailing.Length);
        }

        return new PicParseResult(commands, timeline, stateMachine.GetSnapshot(), trailing);
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

    private static PicCommand ParseCommand(byte[] payload, ref int index, PicOpcode opcode, PicStateMachine state, SCIVersion version)
    {
        return opcode switch
        {
            PicOpcode.SetVisualColor => ParseSetVisual(payload, ref index, state),
            PicOpcode.DisableVisual => ParseDisableVisual(state),
            PicOpcode.SetPriority => ParseSetPriority(payload, ref index, state),
            PicOpcode.DisablePriority => ParseDisablePriority(state),
            PicOpcode.SetControl => ParseSetControl(payload, ref index, state),
            PicOpcode.DisableControl => ParseDisableControl(state),
            PicOpcode.FloodFill => ParseFloodFill(payload, ref index, state),
            PicOpcode.SetPattern => ParseSetPattern(payload, ref index, state),
            PicOpcode.RelativeShortLines => ParseRelativeShortLines(payload, ref index, state),
            PicOpcode.RelativeMediumLines => ParseRelativeMediumLines(payload, ref index, state),
            PicOpcode.RelativeLongLines => ParseRelativeLongLines(payload, ref index, state),
            PicOpcode.RelativePatterns => ParseRelativePatterns(payload, ref index, state, PicOpcode.RelativePatterns),
            PicOpcode.RelativeMediumPatterns => ParseRelativePatterns(payload, ref index, state, PicOpcode.RelativeMediumPatterns),
            PicOpcode.AbsolutePatterns => ParseAbsolutePatterns(payload, ref index, state),
            PicOpcode.ExtendedFunction => ParseExtended(payload, ref index, state),
            _ => ParseUnknown(payload, ref index, opcode)
        };
    }

    private static PicCommand ParseSetVisual(byte[] payload, ref int index, PicStateMachine state)
    {
        var color = ReadByte(payload, ref index);
        state.SetVisual(color);
        var snapshot = state.GetSnapshot();
        return new PicCommand.SetVisual(snapshot.VisualPalette, snapshot.VisualColorIndex);
    }

    private static PicCommand ParseDisableVisual(PicStateMachine state)
    {
        state.DisableVisual();
        return new PicCommand.DisableVisual();
    }

    private static PicCommand ParseSetPriority(byte[] payload, ref int index, PicStateMachine state)
    {
        var value = ReadByte(payload, ref index);
        state.SetPriority(value);
        return new PicCommand.SetPriority((byte)(value & 0x0F));
    }

    private static PicCommand ParseDisablePriority(PicStateMachine state)
    {
        state.DisablePriority();
        return new PicCommand.DisablePriority();
    }

    private static PicCommand ParseSetControl(byte[] payload, ref int index, PicStateMachine state)
    {
        var value = ReadByte(payload, ref index);
        state.SetControl(value);
        return new PicCommand.SetControl(value);
    }

    private static PicCommand ParseDisableControl(PicStateMachine state)
    {
        state.DisableControl();
        return new PicCommand.DisableControl();
    }

    private static PicCommand ParseSetPattern(byte[] payload, ref int index, PicStateMachine state)
    {
        var patternCode = ReadByte(payload, ref index);
        state.SetPattern(patternCode);
        var snapshot = state.GetSnapshot();
        var flags = snapshot.Flags;
        var isRectangle = (flags & PicStateFlags.PatternIsRectangle) != 0;
        var useBrush = (flags & PicStateFlags.PatternUsesBrush) != 0;
        return new PicCommand.SetPattern(snapshot.PatternNumber, snapshot.PatternSize, isRectangle, useBrush, patternCode);
    }

    private static PicCommand ParseFloodFill(byte[] payload, ref int index, PicStateMachine state)
    {
        var color = ReadByte(payload, ref index);
        var x = (ushort)ReadByte(payload, ref index);
        var y = (ushort)ReadByte(payload, ref index);
        state.UpdatePen(x, y);
        return new PicCommand.FloodFill(color, x, y);
    }

    private static PicCommand ParseRelativeShortLines(byte[] payload, ref int index, PicStateMachine state)
    {
        var color = ReadByte(payload, ref index);
        var segments = new List<(int dx, int dy)>();
        var startX = state.GetSnapshot().PenX;
        var startY = state.GetSnapshot().PenY;

        while (index < payload.Length)
        {
            var value = payload[index];
            if (value >= (byte)PicOpcode.SetVisualColor)
            {
                break;
            }

            index++;
            var dxMagnitude = (value >> 4) & 0x07;
            var dx = ((value & 0x80) != 0) ? -dxMagnitude : dxMagnitude;
            var dyMagnitude = value & 0x07;
            var dy = ((value & 0x08) != 0) ? -dyMagnitude : dyMagnitude;
            segments.Add((dx, dy));
            state.OffsetPen(dx, dy);
        }

        var snapshot = state.GetSnapshot();
        return new PicCommand.RelativeLine(PicOpcode.RelativeShortLines, color, segments, startX, startY, snapshot.PenX, snapshot.PenY);
    }

    private static PicCommand ParseRelativeMediumLines(byte[] payload, ref int index, PicStateMachine state)
    {
        var snapshot = state.GetSnapshot();
        var color = snapshot.VisualColorIndex;
        var (startX, startY) = ReadAbsoluteCoordinate(payload, ref index);
        state.UpdatePen(startX, startY);

        var segments = new List<(int dx, int dy)>();
        var currentX = startX;
        var currentY = startY;

        while (index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            var yByte = ReadByte(payload, ref index);
            var nextY = (yByte & 0x80) != 0 ? currentY - (yByte & 0x7F) : currentY + (yByte & 0x7F);
            var xByte = ReadByte(payload, ref index);
            var nextX = (short)(currentX + (sbyte)xByte);
            segments.Add((nextX - currentX, nextY - currentY));
            currentX = nextX;
            currentY = nextY;
        }

        state.UpdatePen(currentX, currentY);
        return new PicCommand.RelativeLine(PicOpcode.RelativeMediumLines, color, segments, startX, startY, currentX, currentY);
    }

    private static PicCommand ParseRelativeLongLines(byte[] payload, ref int index, PicStateMachine state)
    {
        var snapshot = state.GetSnapshot();
        var color = snapshot.VisualColorIndex;
        var (startX, startY) = ReadAbsoluteCoordinate(payload, ref index);
        state.UpdatePen(startX, startY);

        var segments = new List<(int dx, int dy)>();
        var currentX = startX;
        var currentY = startY;

        while (index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            var (nextX, nextY) = ReadAbsoluteCoordinate(payload, ref index);
            segments.Add((nextX - currentX, nextY - currentY));
            currentX = nextX;
            currentY = nextY;
        }

        state.UpdatePen(currentX, currentY);
        return new PicCommand.RelativeLine(PicOpcode.RelativeLongLines, color, segments, startX, startY, currentX, currentY);
    }

    private static PicCommand ParseRelativePatterns(byte[] payload, ref int index, PicStateMachine state, PicOpcode opcode)
    {
        var snapshot = state.GetSnapshot();
        var useBrush = snapshot.Flags.HasFlag(PicStateFlags.PatternUsesBrush);
        var isRectangle = snapshot.Flags.HasFlag(PicStateFlags.PatternIsRectangle);
        var patternNumber = snapshot.PatternNumber;
        var patternSize = snapshot.PatternSize;

        if (useBrush && index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            var patternByte = ReadByte(payload, ref index);
            patternNumber = (byte)((patternByte >> 1) & 0x7F);
            state.UpdatePatternNumber(patternNumber);
        }

        var instances = new List<PatternInstance>();
        var (startX, startY) = ReadAbsoluteCoordinate(payload, ref index);
        instances.Add(new PatternInstance(startX, startY, patternNumber));
        state.UpdatePen(startX, startY);

        var currentX = startX;
        var currentY = startY;

        while (index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            if (useBrush)
            {
                var patternByte = ReadByte(payload, ref index);
                patternNumber = (byte)((patternByte >> 1) & 0x7F);
                state.UpdatePatternNumber(patternNumber);
            }

            var yByte = ReadByte(payload, ref index);
            var nextY = (yByte & 0x80) != 0 ? currentY - (yByte & 0x7F) : currentY + (yByte & 0x7F);
            var xByte = ReadByte(payload, ref index);
            var nextX = (short)(currentX + (sbyte)xByte);

            instances.Add(new PatternInstance(nextX, nextY, patternNumber));
            currentX = nextX;
            currentY = nextY;
            state.UpdatePen(currentX, currentY);
        }

        var updated = state.GetSnapshot();
        return new PicCommand.PatternDraw(opcode, instances, updated.PatternNumber, updated.PatternSize, useBrush, isRectangle);
    }

    private static PicCommand ParseAbsolutePatterns(byte[] payload, ref int index, PicStateMachine state)
    {
        var snapshot = state.GetSnapshot();
        var useBrush = snapshot.Flags.HasFlag(PicStateFlags.PatternUsesBrush);
        var isRectangle = snapshot.Flags.HasFlag(PicStateFlags.PatternIsRectangle);
        var patternNumber = snapshot.PatternNumber;
        var patternSize = snapshot.PatternSize;

        var instances = new List<PatternInstance>();
        var (startX, startY) = ReadAbsoluteCoordinate(payload, ref index);
        instances.Add(new PatternInstance(startX, startY, patternNumber));
        state.UpdatePen(startX, startY);

        while (index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            var (nextX, nextY) = ReadAbsoluteCoordinate(payload, ref index);
            instances.Add(new PatternInstance(nextX, nextY, patternNumber));
            state.UpdatePen(nextX, nextY);
        }

        var updated = state.GetSnapshot();
        return new PicCommand.PatternDraw(PicOpcode.AbsolutePatterns, instances, updated.PatternNumber, updated.PatternSize, useBrush, isRectangle);
    }

    private static PicCommand ParseExtended(byte[] payload, ref int index, PicStateMachine state)
    {
        if (index >= payload.Length)
        {
            return new PicCommand.Extended(PicExtendedOpcode.Unknown, Array.Empty<byte>());
        }

        var subOpcode = (PicExtendedOpcode)payload[index++];
        var data = ReadUntilOpcode(payload, ref index);
        state.ApplyExtended(subOpcode, data);
        return new PicCommand.Extended(subOpcode, data);
    }

    private static PicCommand ParseUnknown(byte[] payload, ref int index, PicOpcode opcode)
    {
        var data = ReadUntilOpcode(payload, ref index);
        return new PicCommand.Unknown(opcode, data);
    }

    private static byte[] ReadUntilOpcode(byte[] payload, ref int index)
    {
        var start = index;
        while (index < payload.Length && payload[index] < (byte)PicOpcode.SetVisualColor)
        {
            index++;
        }

        var length = index - start;
        if (length <= 0)
        {
            return Array.Empty<byte>();
        }

        var data = new byte[length];
        Array.Copy(payload, start, data, 0, length);
        return data;
    }

    private static byte ReadByte(byte[] payload, ref int index)
    {
        if (index >= payload.Length)
        {
            return 0;
        }

        return payload[index++];
    }

    private static (int x, int y) ReadAbsoluteCoordinate(byte[] payload, ref int index)
    {
        var prefix = ReadByte(payload, ref index);
        var lowX = ReadByte(payload, ref index);
        var lowY = ReadByte(payload, ref index);
        var x = lowX | ((prefix & 0xF0) << 4);
        var y = lowY | ((prefix & 0x0F) << 8);
        return (x, y);
    }
}
