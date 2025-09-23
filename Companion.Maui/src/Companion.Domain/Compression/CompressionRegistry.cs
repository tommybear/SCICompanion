using System.Collections.Generic;
using System.Linq;

namespace Companion.Domain.Compression;

public interface ICompressionRegistry
{
    byte[] Decompress(byte[] data, int method, int expectedLength);
}

public sealed class CompressionRegistry : ICompressionRegistry
{
    private readonly IReadOnlyList<ICompressionService> _services;

    public CompressionRegistry(IEnumerable<ICompressionService> services)
    {
        _services = services.ToList();
    }

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        var service = _services.FirstOrDefault(s => s.SupportsMethod(method));
        if (service is null)
        {
            throw new NotSupportedException($"No compression service registered for method {method}.");
        }
        return service.Decompress(data, method, expectedLength);
    }
}
