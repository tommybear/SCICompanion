using System.Collections.Generic;

namespace Companion.Domain.Compression;

public sealed class PassthroughCompressionService : ICompressionService
{
    private readonly HashSet<int> _supportedMethods;

    public PassthroughCompressionService(params int[] methods)
    {
        _supportedMethods = new HashSet<int>(methods);
    }

    public bool SupportsMethod(int method) => _supportedMethods.Contains(method);

    public byte[] Decompress(byte[] data, int method)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported.");
        }

        return data;
    }
}
