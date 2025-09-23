namespace Companion.Domain.Compression;

public interface ICompressionService
{
    bool SupportsMethod(int method);

    byte[] Decompress(byte[] data, int method, int expectedLength);
}
