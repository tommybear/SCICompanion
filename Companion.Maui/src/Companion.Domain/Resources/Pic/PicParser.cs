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

        return new PicParseResult(commands, timeline, stateMachine.GetSnapshot());
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
        return new PicCommand.SetPattern(snapshot.PatternNumber, snapshot.PatternSize, isRectangle, useBrush);
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
}
