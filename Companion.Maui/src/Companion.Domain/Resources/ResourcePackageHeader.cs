using System.Buffers.Binary;
using System.IO;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed record ResourcePackageHeader(
    ResourceType Type,
    int Number,
    int Package,
    int CompressedLength,
    int DecompressedLength,
    int CompressionMethod)
{
    public int GetHeaderSize(SCIVersion version) => version switch
    {
        SCIVersion.SCI0 => 8,
        SCIVersion.SCI1 or SCIVersion.SCI11 => 9,
        _ => throw new NotSupportedException($"Unsupported SCI version '{version}'.")
    };

    public void WriteTo(Stream destination, SCIVersion version)
    {
        Span<byte> buffer = stackalloc byte[GetHeaderSize(version)];
        switch (version)
        {
            case SCIVersion.SCI0:
                ushort typeAndNumber = (ushort)(((int)Type << 11) | (Number & 0x7FF));
                BinaryPrimitives.WriteUInt16LittleEndian(buffer, typeAndNumber);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[2..], (ushort)CompressedLength);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[4..], (ushort)DecompressedLength);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[6..], (ushort)CompressionMethod);
                break;
            case SCIVersion.SCI1:
            case SCIVersion.SCI11:
                buffer[0] = (byte)(((int)Type & 0x1F) | 0x80);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[1..], (ushort)Number);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[3..], (ushort)CompressedLength);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[5..], (ushort)DecompressedLength);
                BinaryPrimitives.WriteUInt16LittleEndian(buffer[7..], (ushort)CompressionMethod);
                break;
            default:
                throw new NotSupportedException($"Unsupported SCI version '{version}'.");
        }

        destination.Write(buffer);
    }
}
