using System;

namespace Companion.Domain.Compression;

public sealed class LzwViewCompressionService : ICompressionService
{
    private readonly Func<byte[], int, int, byte[]> _baseDecoder;
    private readonly int[] _supportedMethods;

    public LzwViewCompressionService(Lzw1CompressionService baseService, params int[] methods)
        : this((data, _, length) => baseService.Decompress(data, 2, length), methods)
    {
    }

    internal LzwViewCompressionService(Func<byte[], int, int, byte[]> baseDecoder, params int[] methods)
    {
        _baseDecoder = baseDecoder ?? throw new ArgumentNullException(nameof(baseDecoder));
        _supportedMethods = methods.Length > 0 ? methods : new[] { 3 };
    }

    public bool SupportsMethod(int method) => Array.IndexOf(_supportedMethods, method) >= 0;

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported by LZW_View.");
        }

        var intermediate = _baseDecoder(data, 2, expectedLength);
        var output = new byte[expectedLength];
        CompressionReorder.ReorderView(intermediate, output);
        return output;
    }
}
