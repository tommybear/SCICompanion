using Xunit;
using Companion.Domain.Compression;

namespace Companion.Application.Tests;

public class LzwCompressionServiceTests
{
    [Fact]
    public void Decompress_LiteralsOnly()
    {
        var service = new LzwCompressionService(1);
        var registry = new CompressionRegistry(new ICompressionService[]
        {
            service
        });

        // Tokens: 0x41, 0x42, 0x43, 0x101 (terminator)
        var compressed = new byte[] { 65, 132, 12, 9, 8 };
        var result = registry.Decompress(compressed, 1, 3);

        Assert.Equal(new byte[] { 0x41, 0x42, 0x43 }, result);
    }
}
