using System;
using System.Collections.Generic;
using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Xunit;

namespace Companion.Application.Tests;

public class CompressionFixtureTests
{
    [Fact]
    public void Sci0_Pic_MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI0", ResourceType.Pic, 1);
        var expected = Convert.FromBase64String("/gEAABEiM0RVZneImaq7zN3uiIgBAgMEBQaIiPn6+/z9/v8IkSo7TF1uiP4BAQABAgMEBQYHCIGCg4SF5oeHcXJzdHV2eIeJiouMjY6P+PH6O/z9/vj+AQIAURIjJGXGNwhZGissbc64iNGSo6RlRjh42ZqrrG1OP3+U2uu8TVSI/gEDYGFiY2RlZmfo6err7O3u6EhBQkNERUZIyMnKy8zNzs8YERoLHB0eGPkA8fP8/wAwEyU=");
        Assert.Equal(0, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_Pic_MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.Pic, 0);
        var expected = Convert.FromBase64String("JgAQDgAAoADw2EYACQAAAF0AAAAAAAAAAAAAAEoAAABEAAAAAAAAACoANQBAAEoAVQBfAGoAdAB/AIoAlACfAKkAtAAAAw0AAAAMAPjR4IlG+rgBAAAAAAUJAAAA8ADz/PgAcmb/");
        Assert.Equal(20, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_Text10_Dcl18MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.Text, 10);
        var expected = Convert.FromBase64String("JWQAY2xhc3M6ICVzCnZpZXc6ICVkCmxvb3A6ICVkCmNlbDogJWQKcG9zbjogJWQgJWQgJWQKaGVhZGluZzogJWQKcHJpOiAlZApzaWduYWw6ICQleAppbGxCaXRzOiAkJXgKAG5hbWU6ICVzCnZpZXc6ICVkCmxvb3A6ICVkCmNlbDogJWQKcG9zbjogJWQgJWQgJWQKaGVhZGluZzogJWQKcHJpOiAlZApzaWduYWw6ICQleAppbGxCaXRzOiAkJXgKT25Db250cm9sOiAkJXgKT3JpZ2luIG9uOiAkJXggAG5hbWU6ICVzCm51bWJlcjogJWQKY3VycmVudCBwaWM6ICVkCnN0eWxlOiAlZApob3Jpem9uOiAlZApub3J0aDogJWQKc291dGg6ICVkCmVhc3Q6ICVkCndlc3Q6ICVkCnNjcmlwdDogJXMgACVkLyVkAA==");
        Assert.Equal(18, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci11_View981_Dcl19MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI1.1", ResourceType.View, 981);
        var expected = Convert.FromBase64String("EAABAAEAAQAAAAAAECQAAAAA/wAB/////wMAAAAAIgAAABEACAADAAQAPwoAAGIAAAAjAAAAAAAAAEwAAABvAAAAAAAAAAAEYgAAAMSGx8IChQPFBYMFxAaDggPDBYSDAsMFgwbDBYQExMOChILGBgUEAAMEBj8FAwAFAAQAAAQFAwAHBQYHAAUDAAQAAwUFAwAFAwUEBQMDBQAFAwMABQQ/BAUEAAMABAUEBAUE");
        Assert.Equal(19, method);
        Assert.Equal(expected, data);
    }

    [Fact]
    public void Sci0_Sound2_MatchesFixture()
    {
        var (data, method) = DecompressResource("SCI0", ResourceType.Sound, 2);
        var actualBase64 = Convert.ToBase64String(data);
        Assert.Equal(0, method);
        Assert.Equal("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", actualBase64);
    }

    [Fact]
    public void StacPack_CopySequence_MatchesFixture()
    {
        var compressed = BuildStacSample();
        var service = new StacCompressionService(32);
        var result = service.Decompress(compressed, 32, 6);
        var expected = new byte[] { (byte)'A', (byte)'B', (byte)'C', (byte)'A', (byte)'B', (byte)'C' };
        Assert.Equal(expected, result);
    }

    [Fact]
    public void LzwPic_Reorder_FixtureMatches()
    {
        var (intermediate, _, _) = CompressionTestData.BuildPicIntermediate();
        var expected = new byte[intermediate.Length];
        CompressionReorder.ReorderPic(intermediate, expected);

        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new LzwPicCompressionService((_, _, _) => intermediate, 4)
        });

        var codec = new PicResourceCodec(registry);
        var header = new ResourcePackageHeader(ResourceType.Pic, 630, 0, 0, expected.Length, 4);
        var package = new ResourcePackage(SCIVersion.SCI11, header, Array.Empty<byte>());

        var decoded = codec.Decode(package);
        var reordered = Assert.IsType<byte[]>(decoded.Metadata["PicDecodedPayload"]);
        const string expectedBase64 = "/gIAAQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/AAAAAAABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJmam5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8DBwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5ufo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v8AAQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHyAhIiMkJSYnKCkqKywtLi8wMTIzNDU2Nzg5Ojs8PT4/QEFCQ0RFRkdISUpLTE1OT1BRUlNUVVZXWFlaW1xdXl9gYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXp7fH1+f4CBgoOEhYaHiImKi4yNjo+QkZKTlJWWl5iZmpucnZ6foKGio6SlpqeoqaqrrK2ur7CxsrO0tba3uLm6u7y9vr/AwcLDxMXGx8jJysvMzc7P0NHS09TV1tfY2drb3N3e3+Dh4uPk5ebn6Onq6+zt7u/w8fLz9PX29/j5+vv8/f7/AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/wABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fICEiIyQlJicoKSorLC0uLzAxMjM0NTY3ODk6Ozw9Pj9AQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVpbXF1eX2BhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ent8fX5/gIGCg4SFhoeIiYqLjI2Oj5CRkpOUlZaXmJmam5ydnp+goaKjpKWmp6ipqqusra6vsLGys7S1tre4ubq7vL2+v8DBwsPExcbHyMnKy8zNzs/Q0dLT1NXW19jZ2tvc3d7f4OHi4+Tl5ufo6err7O3u7/Dx8vP09fb3+Pn6+/z9/v/+AQAAAAwACgsMDQ4PEADAwMDA";

        Assert.Equal(expectedBase64, Convert.ToBase64String(reordered));
        Assert.Equal(expectedBase64, Convert.ToBase64String(expected));
    }

    private static (byte[] Data, int Method) DecompressResource(string folderName, ResourceType type, int number)
    {
        var discovery = new ResourceDiscoveryService();
        var reader = new ResourceVolumeReader();
        var registry = BuildCompressionRegistry();

        var folder = RepoPaths.GetTemplateGame(folderName);
        var catalog = discovery.Discover(folder);
        var descriptor = catalog.Resources.First(r => r.Type == type && r.Number == number);
        var package = reader.Read(folder, descriptor, catalog.Version);
        var method = package.Header.CompressionMethod;
        var data = method == 0
            ? package.Body
            : registry.Decompress(package.Body, method, package.Header.DecompressedLength);
        return (data, method);
    }

    private static CompressionRegistry BuildCompressionRegistry()
    {
        var lzw1 = new Lzw1CompressionService(2);
        return new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1),
            lzw1,
            new LzwViewCompressionService(lzw1, 3),
            new LzwPicCompressionService(lzw1, 4),
            new DclCompressionService(8, 18, 19, 20),
            new StacCompressionService(32)
        });
    }

    private static byte[] BuildStacSample()
    {
        var bits = new List<int>();
        WriteLiteral(bits, 0x41);
        WriteLiteral(bits, 0x42);
        WriteLiteral(bits, 0x43);
        WriteShortCopy(bits, 3, 3);
        return PackBits(bits);
    }

    private static void WriteLiteral(List<int> bits, int value)
    {
        bits.Add(0);
        for (var bit = 7; bit >= 0; bit--)
        {
            bits.Add((value >> bit) & 1);
        }
    }

    private static void WriteShortCopy(List<int> bits, int offset, int length)
    {
        bits.Add(1);
        bits.Add(1);
        for (var bit = 6; bit >= 0; bit--)
        {
            bits.Add((offset >> bit) & 1);
        }
        WriteLength(bits, length);
    }

    private static void WriteLength(List<int> bits, int length)
    {
        if (length >= 2 && length <= 4)
        {
            var value = length - 2;
            bits.Add((value >> 1) & 1);
            bits.Add(value & 1);
            return;
        }

        bits.Add(1);
        bits.Add(1);

        if (length >= 5 && length <= 7)
        {
            var value = length - 5;
            bits.Add((value >> 1) & 1);
            bits.Add(value & 1);
            return;
        }

        // Variable length encoding (length >= 8)
        bits.Add(1);
        bits.Add(1);
        var remaining = length - 8;
        while (remaining >= 15)
        {
            WriteNibble(bits, 0xF);
            remaining -= 15;
        }
        WriteNibble(bits, remaining);
    }

    private static void WriteNibble(List<int> bits, int value)
    {
        for (var bit = 3; bit >= 0; bit--)
        {
            bits.Add((value >> bit) & 1);
        }
    }

    private static byte[] PackBits(List<int> bits)
    {
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
