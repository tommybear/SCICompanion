using Xunit;
using System;
using Companion.Application.Tests.Support;
using Companion.Domain.Compression;

namespace Companion.Application.Tests;

public class CompressionRegistryTests
{
    [Fact]
    public void Passthrough_ReturnsSameBytes()
    {
        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1),
            new LzwPicCompressionService((_, _, length) => new byte[length], 4),
            new DclCompressionService(8, 18, 19, 20)
        });

        var data = new byte[] { 1, 2, 3 };
        var result = registry.Decompress(data, 0, data.Length);

        Assert.Equal(data, result);
    }

    [Fact]
    public void RegistryThrows_WhenNoServiceSupportsMethod()
    {
        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0),
            new LzwCompressionService(1)
        });

        Assert.Throws<NotSupportedException>(() => registry.Decompress(Array.Empty<byte>(), 5, 0));
    }

    [Fact]
    public void RegistryRoutesLzwPic()
    {
        var (intermediate, _, _) = CompressionTestData.BuildPicIntermediate();
        var expected = new byte[intermediate.Length];
        CompressionReorder.ReorderPic(intermediate, expected);

        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new LzwPicCompressionService((_, _, length) => intermediate, 4)
        });

        var actual = registry.Decompress(Array.Empty<byte>(), 4, intermediate.Length);

        Assert.Equal(expected, actual);
    }
}
