using System;
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

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported.");
        }

        if (expectedLength < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(expectedLength));
        }

        if (expectedLength > data.Length)
        {
            throw new InvalidOperationException("Expected length exceeds raw payload size for passthrough compression.");
        }

        if (expectedLength == data.Length)
        {
            return data.ToArray();
        }

        var result = new byte[expectedLength];
        Array.Copy(data, result, expectedLength);
        return result;
    }
}
