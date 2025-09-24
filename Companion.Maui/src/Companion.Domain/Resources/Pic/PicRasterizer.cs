using System;
using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources.Pic;

internal static class PicRasterizer
{
    public static PicDocument Render(PicParseResult parseResult, SCIVersion version)
    {
        var (width, height) = PicDimensions.GetSizeForVersion(version);
        var visual = new byte[width * height];
        var priority = new byte[width * height];
        var control = new byte[width * height];

        var currentState = CreateInitialState(version);

        for (var i = 0; i < parseResult.Commands.Count; i++)
        {
            var command = parseResult.Commands[i];
            currentState = ApplyCommand(command, currentState, version, visual, priority, control, width, height);
        }

        return new PicDocument(
            width,
            height,
            parseResult.Commands,
            parseResult.StateTimeline,
            parseResult.FinalState,
            visual,
            priority,
            control);
    }

    private static PicStateSnapshot CreateInitialState(SCIVersion version)
    {
        var stateMachine = new PicStateMachine(version);
        return stateMachine.GetSnapshot();
    }

    private static PicStateSnapshot ApplyCommand(
        PicCommand command,
        PicStateSnapshot state,
        SCIVersion version,
        byte[] visual,
        byte[] priority,
        byte[] control,
        int width,
        int height)
    {
        switch (command)
        {
            case PicCommand.SetVisual setVisual:
                return state with
                {
                    Flags = state.Flags | PicStateFlags.VisualEnabled,
                    VisualPalette = setVisual.Palette,
                    VisualColorIndex = setVisual.ColorIndex
                };
            case PicCommand.DisableVisual:
                return state with { Flags = state.Flags & ~PicStateFlags.VisualEnabled };
            case PicCommand.SetPriority setPriority:
                return state with
                {
                    Flags = state.Flags | PicStateFlags.PriorityEnabled,
                    PriorityValue = (byte)(setPriority.Value & 0x0F)
                };
            case PicCommand.DisablePriority:
                return state with { Flags = state.Flags & ~PicStateFlags.PriorityEnabled };
            case PicCommand.SetControl setControl:
                return state with
                {
                    Flags = state.Flags | PicStateFlags.ControlEnabled,
                    ControlValue = setControl.Value
                };
            case PicCommand.DisableControl:
                return state with { Flags = state.Flags & ~PicStateFlags.ControlEnabled };
            case PicCommand.SetPattern setPattern:
                var flags = state.Flags;
                flags = setPattern.UseBrush ? flags | PicStateFlags.PatternUsesBrush : flags & ~PicStateFlags.PatternUsesBrush;
                flags = setPattern.IsRectangle ? flags | PicStateFlags.PatternIsRectangle : flags & ~PicStateFlags.PatternIsRectangle;
                return state with
                {
                    Flags = flags,
                    PatternNumber = setPattern.PatternNumber,
                    PatternSize = setPattern.PatternSize
                };
            case PicCommand.RelativeLine line:
                DrawLine(line, state, version, visual, priority, control, width, height);
                return state with { PenX = line.EndX, PenY = line.EndY };
            case PicCommand.FloodFill fill:
                FloodFill(fill, state, version, visual, priority, control, width, height);
                return state with { PenX = fill.X, PenY = fill.Y };
            default:
                return state;
        }
    }

    private static void DrawLine(
        PicCommand.RelativeLine line,
        PicStateSnapshot state,
        SCIVersion version,
        byte[] visual,
        byte[] priority,
        byte[] control,
        int width,
        int height)
    {
        var colorValue = GetVisualColorValue(state, line.ColorIndex, version);
        var drawVisual = state.Flags.HasFlag(PicStateFlags.VisualEnabled);
        var drawPriority = state.Flags.HasFlag(PicStateFlags.PriorityEnabled);
        var drawControl = state.Flags.HasFlag(PicStateFlags.ControlEnabled);

        var x = state.PenX;
        var y = state.PenY;

        foreach (var (dx, dy) in line.Segments)
        {
            var targetX = x + dx;
            var targetY = y + dy;
            PlotSegment(x, y, targetX, targetY, width, height, drawVisual ? visual : null, drawPriority ? priority : null, drawControl ? control : null, colorValue, state.PriorityValue, state.ControlValue);
            x = targetX;
            y = targetY;
        }
    }

    private static void PlotSegment(
        int startX,
        int startY,
        int endX,
        int endY,
        int width,
        int height,
        byte[]? visual,
        byte[]? priority,
        byte[]? control,
        byte color,
        byte priorityValue,
        byte controlValue)
    {
        var dx = Math.Abs(endX - startX);
        var sx = startX < endX ? 1 : -1;
        var dy = -Math.Abs(endY - startY);
        var sy = startY < endY ? 1 : -1;
        var error = dx + dy;
        var x = startX;
        var y = startY;

        while (true)
        {
            PlotPixel(x, y, width, height, visual, priority, control, color, priorityValue, controlValue);
            if (x == endX && y == endY)
            {
                break;
            }
            var e2 = 2 * error;
            if (e2 >= dy)
            {
                error += dy;
                x += sx;
            }
            if (e2 <= dx)
            {
                error += dx;
                y += sy;
            }
        }
    }

    private static void FloodFill(
        PicCommand.FloodFill fill,
        PicStateSnapshot state,
        SCIVersion version,
        byte[] visual,
        byte[] priority,
        byte[] control,
        int width,
        int height)
    {
        if (!state.Flags.HasFlag(PicStateFlags.VisualEnabled))
        {
            return;
        }

        var targetColor = GetVisualColorAt(visual, width, height, fill.X, fill.Y);
        var fillColor = GetVisualColorValue(state, fill.ColorIndex, version);
        if (targetColor == fillColor)
        {
            return;
        }

        var stack = new Stack<(int x, int y)>();
        stack.Push((fill.X, fill.Y));

        while (stack.Count > 0)
        {
            var (x, y) = stack.Pop();
            if (!IsInBounds(x, y, width, height))
            {
                continue;
            }

            var index = y * width + x;
            if (visual[index] != targetColor)
            {
                continue;
            }

            visual[index] = fillColor;
            if (state.Flags.HasFlag(PicStateFlags.PriorityEnabled))
            {
                priority[index] = state.PriorityValue;
            }
            if (state.Flags.HasFlag(PicStateFlags.ControlEnabled))
            {
                control[index] = state.ControlValue;
            }

            stack.Push((x + 1, y));
            stack.Push((x - 1, y));
            stack.Push((x, y + 1));
            stack.Push((x, y - 1));
        }
    }

    private static byte GetVisualColorValue(PicStateSnapshot state, byte colorIndex, SCIVersion version)
    {
        if (version <= SCIVersion.SCI0)
        {
            return (byte)(state.VisualPalette * 40 + colorIndex);
        }
        return colorIndex;
    }

    private static byte GetVisualColorAt(byte[] visual, int width, int height, int x, int y)
    {
        if (!IsInBounds(x, y, width, height))
        {
            return 0;
        }
        return visual[y * width + x];
    }

    private static bool IsInBounds(int x, int y, int width, int height) => x >= 0 && y >= 0 && x < width && y < height;

    private static void PlotPixel(int x, int y, int width, int height, byte[]? visual, byte[]? priority, byte[]? control, byte color, byte priorityValue, byte controlValue)
    {
        if (!IsInBounds(x, y, width, height))
        {
            return;
        }

        var index = y * width + x;
        if (visual is not null)
        {
            visual[index] = color;
        }
        if (priority is not null)
        {
            priority[index] = priorityValue;
        }
        if (control is not null)
        {
            control[index] = controlValue;
        }
    }
}
