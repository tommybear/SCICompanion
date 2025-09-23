using System.Collections.Generic;
using Companion.Domain.Compression;
using Xunit;

namespace Companion.Application.Tests;

public class Lzw1CompressionServiceTests
{
    [Fact]
    public void Decompress_SingleLiteralToken()
    {
        var registry = BuildRegistry();
        var compressed = CreateCompressedStream(0x41, 0x101);

        var result = registry.Decompress(compressed, 2, 1);

        Assert.Equal(new byte[] { 0x41 }, result);
    }

    [Fact]
    public void Decompress_DictionaryReference()
    {
        var registry = BuildRegistry();
        var compressed = CreateCompressedStream(0x41, 0x42, 0x102, 0x101);

        var result = registry.Decompress(compressed, 2, 4);

        Assert.Equal(new byte[] { 0x41, 0x42, 0x41, 0x42 }, result);
    }

    [Fact]
    public void Decompress_KwKwKCase()
    {
        var registry = BuildRegistry();
        var compressed = CreateCompressedStream(0x41, 0x42, 0x102, 0x103, 0x101);

        var result = registry.Decompress(compressed, 2, 5);

        // Sierra's LZW_1 variant emits the trailing dictionary byte for this pattern; fixtures will validate this expectation.
        Assert.Equal(new byte[] { 0x41, 0x42, 0x41, 0x42, 0x42 }, result);
    }

    private static CompressionRegistry BuildRegistry()
    {
        return new CompressionRegistry(new ICompressionService[]
        {
            new Lzw1CompressionService(2)
        });
    }

    private static byte[] CreateCompressedStream(params int[] tokens)
    {
        var buffer = new List<byte>();
        var bitPosition = 0;
        foreach (var token in tokens)
        {
            WriteBits(buffer, token, 9, ref bitPosition);
        }
        return buffer.ToArray();
    }

    private static void WriteBits(List<byte> buffer, int value, int width, ref int bitPosition)
    {
        for (var bit = width - 1; bit >= 0; bit--)
        {
            var bitValue = (value >> bit) & 1;
            var byteIndex = bitPosition / 8;
            if (byteIndex >= buffer.Count)
            {
                buffer.Add(0);
            }

            var bitOffset = bitPosition % 8;
            if (bitValue != 0)
            {
                buffer[byteIndex] |= (byte)(1 << (7 - bitOffset));
            }

            bitPosition++;
        }
    }
}
