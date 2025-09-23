using System.Collections.Generic;
using Companion.Domain.Compression;
using Xunit;

namespace Companion.Application.Tests;

public class StacCompressionServiceTests
{
    [Fact]
    public void Decompress_LiteralStream()
    {
        var service = new StacCompressionService(32);
        var compressed = BuildLiteralStream(0x41, 0x42, 0x43);
        var result = service.Decompress(compressed, 32, 3);

        Assert.Equal(new byte[] { 0x41, 0x42, 0x43 }, result);
    }

    private static byte[] BuildLiteralStream(params byte[] values)
    {
        var bits = new List<int>();
        foreach (var value in values)
        {
            bits.Add(0); // literal flag
            for (var bit = 7; bit >= 0; bit--)
            {
                bits.Add((value >> bit) & 1);
            }
        }

        var bytes = new List<byte>();
        var current = 0;
        var bitCount = 0;
        foreach (var bit in bits)
        {
            current = (current << 1) | bit;
            bitCount++;
            if (bitCount == 8)
            {
                bytes.Add((byte)current);
                current = 0;
                bitCount = 0;
            }
        }

        if (bitCount > 0)
        {
            current <<= 8 - bitCount;
            bytes.Add((byte)current);
        }

        return bytes.ToArray();
    }
}
