using Xunit;
using System;
using Companion.Domain.Compression;

namespace Companion.Application.Tests;

public class CompressionRegistryTests
{
    [Fact]
    public void Passthrough_ReturnsSameBytes()
    {
        var registry = new CompressionRegistry(new ICompressionService[]
        {
            new PassthroughCompressionService(0, 20),
            new LzwCompressionService(1)
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
            new PassthroughCompressionService(0, 20),
            new LzwCompressionService(1)
        });

        Assert.Throws<NotSupportedException>(() => registry.Decompress(Array.Empty<byte>(), 5, 0));
    }
}
