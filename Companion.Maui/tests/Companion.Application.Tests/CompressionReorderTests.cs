using System;
using System.Buffers.Binary;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Xunit;

namespace Companion.Application.Tests;

public class CompressionReorderTests
{
    [Fact]
    public void ReorderPic_WritesBitmapCommand()
    {
        var (intermediate, viewStart, viewSize) = CompressionTestData.BuildPicIntermediate();
        var destination = new byte[intermediate.Length];

        CompressionReorder.ReorderPic(intermediate, destination);

        Assert.Equal(0xFE, destination[0]);
        Assert.Equal(0x02, destination[1]);
        for (var i = 0; i < 256; i++)
        {
            Assert.Equal((byte)i, destination[2 + i]);
        }

        var drawIndex = viewStart;
        Assert.Equal(0xFE, destination[drawIndex]);
        Assert.Equal(0x01, destination[drawIndex + 1]);
        Assert.Equal((ushort)(viewSize + 8), BinaryPrimitives.ReadUInt16LittleEndian(destination.AsSpan(drawIndex + 5, 2)));

        var decoded = destination.AsSpan(drawIndex + 15, viewSize).ToArray();
        Assert.Equal(new byte[] { 0xC0, 0xC0, 0xC0, 0xC0 }, decoded);
    }

    [Fact]
    public void ReorderView_WritesCelData()
    {
        var (intermediate, expectedCelOffset) = CompressionTestData.BuildViewIntermediate();
        var destination = new byte[intermediate.Length];

        CompressionReorder.ReorderView(intermediate, destination);

        Assert.Equal(1, destination[0]);
        Assert.Equal(0x80, destination[1]);
        var celOffset = BinaryPrimitives.ReadUInt16LittleEndian(destination.AsSpan(8, 2));
        Assert.True(celOffset > 8, "Cel offset should follow header metadata.");
        Assert.Equal((ushort)1, BinaryPrimitives.ReadUInt16LittleEndian(destination.AsSpan(10, 2)));
        var celPointer = BinaryPrimitives.ReadUInt16LittleEndian(destination.AsSpan(14, 2));
        Assert.True(celPointer >= celOffset, "Cel pointer table should reference a position at or beyond the cel metadata block.");
        var celData = destination.AsSpan(celPointer + 8, 2).ToArray();
        Assert.Equal(new byte[] { 0xC0, 0xC0 }, celData);
    }

    [Fact]
    public void LzwPicCompressionService_AppliesReorder()
    {
        var (intermediate, viewStart, viewSize) = CompressionTestData.BuildPicIntermediate();
        var expected = new byte[intermediate.Length];
        CompressionReorder.ReorderPic(intermediate, expected);

        var service = new LzwPicCompressionService((_, _, length) => intermediate, 4);
        var actual = service.Decompress(Array.Empty<byte>(), 4, intermediate.Length);

        Assert.Equal(expected, actual);
        Assert.Equal(0xFE, actual[viewStart]);
        Assert.Equal((ushort)(viewSize + 8), BinaryPrimitives.ReadUInt16LittleEndian(actual.AsSpan(viewStart + 5, 2)));
    }

    [Fact]
    public void LzwViewCompressionService_AppliesReorder()
    {
        var (intermediate, _) = CompressionTestData.BuildViewIntermediate();
        var expected = new byte[intermediate.Length];
        CompressionReorder.ReorderView(intermediate, expected);

        var service = new LzwViewCompressionService((_, _, length) => intermediate, 3);
        var actual = service.Decompress(Array.Empty<byte>(), 3, intermediate.Length);

        Assert.Equal(expected, actual);
    }

}
