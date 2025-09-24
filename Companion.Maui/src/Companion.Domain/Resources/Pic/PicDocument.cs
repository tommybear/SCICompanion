using System;
using System.Collections.Generic;

namespace Companion.Domain.Resources.Pic;

public sealed class PicDocument
{
    public PicDocument(
        int width,
        int height,
        IReadOnlyList<PicCommand> commands,
        IReadOnlyList<PicStateSnapshot> stateTimeline,
        PicStateSnapshot finalState,
        byte[] visualPlane,
        byte[] priorityPlane,
        byte[] controlPlane,
        byte[] trailingData)
    {
        Width = width;
        Height = height;
        Commands = commands;
        StateTimeline = stateTimeline;
        FinalState = finalState;
        VisualPlane = visualPlane;
        PriorityPlane = priorityPlane;
        ControlPlane = controlPlane;
        TrailingData = trailingData ?? Array.Empty<byte>();
    }

    public int Width { get; }
    public int Height { get; }
    public IReadOnlyList<PicCommand> Commands { get; }
    public IReadOnlyList<PicStateSnapshot> StateTimeline { get; }
    public PicStateSnapshot FinalState { get; }
    public byte[] VisualPlane { get; }
    public byte[] PriorityPlane { get; }
    public byte[] ControlPlane { get; }
    public byte[] TrailingData { get; }
}

internal static class PicDimensions
{
    public static (int Width, int Height) GetSizeForVersion(Projects.SCIVersion version)
    {
        // SCI0/SCI1.x pictures use a 320x190 canvas.
        return (320, 190);
    }
}
