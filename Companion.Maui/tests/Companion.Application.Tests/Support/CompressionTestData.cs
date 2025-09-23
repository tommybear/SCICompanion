using System;
using System.Buffers.Binary;

namespace Companion.Application.Tests.Support;

internal static class CompressionTestData
{
    public static (byte[] intermediate, int viewStart, int viewSize) BuildPicIntermediate()
    {
        const ushort viewSize = 4;
        const ushort viewStart = 1286;
        const ushort cdataSize = 0;
        var viewHeader = new byte[] { 10, 11, 12, 13, 14, 15, 16 };
        var palette = new byte[4 * 256];
        for (var i = 0; i < palette.Length; i++)
        {
            palette[i] = (byte)(i & 0xFF);
        }
        var rle = new byte[] { 0xC0, 0xC0, 0xC0, 0xC0 };
        var length = viewStart + 15 + viewSize;
        var buffer = new byte[length];
        var index = 0;
        BinaryPrimitives.WriteUInt16LittleEndian(buffer.AsSpan(index, 2), viewSize);
        index += 2;
        BinaryPrimitives.WriteUInt16LittleEndian(buffer.AsSpan(index, 2), viewStart);
        index += 2;
        BinaryPrimitives.WriteUInt16LittleEndian(buffer.AsSpan(index, 2), cdataSize);
        index += 2;
        viewHeader.CopyTo(buffer.AsSpan(index, viewHeader.Length));
        index += viewHeader.Length;
        palette.CopyTo(buffer.AsSpan(index, palette.Length));
        index += palette.Length;
        rle.CopyTo(buffer.AsSpan(index, rle.Length));
        return (buffer, viewStart, viewSize);
    }

    public static (byte[] intermediate, int celOffset) BuildViewIntermediate()
    {
        var data = new System.Collections.Generic.List<byte>();
        data.Add(0);
        data.Add(0);
        data.Add(1);
        data.Add(1);
        data.AddRange(BitConverter.GetBytes((ushort)0));
        data.AddRange(BitConverter.GetBytes((ushort)0));
        data.AddRange(BitConverter.GetBytes((ushort)0));
        data.AddRange(BitConverter.GetBytes((ushort)1));
        data.Add(1);
        data.AddRange(new byte[] { 1, 2, 3, 4, 5, 6 });
        data.Add(1);

        var cellLengthsStart = data.Count;
        data.AddRange(BitConverter.GetBytes((ushort)2));
        data.Add(0xC0);
        data.Add(0xC0);
        data.Add(0);
        data.Add(0);

        var pointerValue = (ushort)(cellLengthsStart - 2);
        data[0] = (byte)(pointerValue & 0xFF);
        data[1] = (byte)(pointerValue >> 8);

        var buffer = data.ToArray();
        if (buffer.Length < 26)
        {
            Array.Resize(ref buffer, 26);
        }

        const int celOffset = 16;
        return (buffer, celOffset);
    }
}
