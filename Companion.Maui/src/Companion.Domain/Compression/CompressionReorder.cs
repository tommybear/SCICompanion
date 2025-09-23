using System;
using System.Buffers.Binary;
using System.IO;

namespace Companion.Domain.Compression;

internal static class CompressionReorder
{
    private const byte ViewHeaderColors8Bit = 0x80;
    private const byte PicOpOpx = 0xFE;
    private const byte PicOpxSc1SetPalette = 0x02;
    private const byte PicOpxSc1DrawBitmap = 0x01;
    private const int PaletteEntryBytes = 4 * 256;
    private const int PicPaletteHeaderSize = 1284;
    private const int PicExtraMagicSize = 15;

    public static void ReorderPic(ReadOnlySpan<byte> source, Span<byte> destination)
    {
        if (destination.Length == 0)
        {
            return;
        }

        var src = source.ToArray();
        var dest = destination;
        var seeker = 0;
        var writer = 0;

        EnsureCapacity(dest.Length, PicPaletteHeaderSize);

        dest[writer++] = PicOpOpx;
        dest[writer++] = PicOpxSc1SetPalette;

        for (var i = 0; i < 256; i++)
        {
            dest[writer++] = (byte)i;
        }

        WriteUInt32(dest, writer, 0);
        writer += 4;

        var viewSize = ReadUInt16(src, ref seeker);
        var viewStart = ReadUInt16(src, ref seeker);
        var compressedPixelSize = ReadUInt16(src, ref seeker);

        Span<byte> viewHeader = stackalloc byte[7];
        new ReadOnlySpan<byte>(src, seeker, viewHeader.Length).CopyTo(viewHeader);
        seeker += viewHeader.Length;

        CopyBytes(src, ref seeker, dest, ref writer, PaletteEntryBytes);

        var paletteGap = viewStart - PicPaletteHeaderSize - 2;
        if (paletteGap > 0)
        {
            CopyBytes(src, ref seeker, dest, ref writer, paletteGap);
        }

        var tailOffset = (int)viewStart + PicExtraMagicSize + viewSize;
        if (destination.Length < tailOffset)
        {
            throw new InvalidDataException("PIC reorder destination is smaller than expected");
        }

        var trailingBytes = destination.Length - tailOffset;
        if (trailingBytes > 0)
        {
            CopyBytes(src, ref seeker, dest, tailOffset, trailingBytes);
        }

        var pixelData = compressedPixelSize > 0 ? new byte[compressedPixelSize] : Array.Empty<byte>();
        if (compressedPixelSize > 0)
        {
            Array.Copy(src, seeker, pixelData, 0, compressedPixelSize);
            seeker += compressedPixelSize;
        }

        var drawIndex = (int)viewStart;
        dest[drawIndex++] = PicOpOpx;
        dest[drawIndex++] = PicOpxSc1DrawBitmap;
        dest[drawIndex++] = 0;
        dest[drawIndex++] = 0;
        dest[drawIndex++] = 0;
        WriteUInt16(dest, drawIndex, (ushort)(viewSize + 8));
        drawIndex += 2;

        viewHeader.CopyTo(dest.Slice(drawIndex, viewHeader.Length));
        drawIndex += viewHeader.Length;
        dest[drawIndex++] = 0;

        var pixelIndex = 0;
        DecodeRle(src, ref seeker, pixelData, ref pixelIndex, dest.Slice(drawIndex, viewSize), viewSize);
    }

    public static void ReorderView(ReadOnlySpan<byte> source, Span<byte> destination)
    {
        if (destination.Length == 0)
        {
            return;
        }

        var src = source.ToArray();
        var dest = destination;
        var seeker = 0;

        var cellLengthsOffset = ReadUInt16(src, ref seeker);
        var cellLengthsIndex = cellLengthsOffset + 2;

        if (cellLengthsIndex < 0 || cellLengthsIndex + 2 > src.Length)
        {
            throw new InvalidDataException("View reorder encountered invalid cell length pointer.");
        }

        var loopHeaders = src[seeker++];
        var loopHeaderPresence = src[seeker++];
        var loopMask = ReadUInt16(src, ref seeker);
        var unknown = ReadUInt16(src, ref seeker);
        var paletteOffset = ReadUInt16(src, ref seeker);
        var celTotal = ReadUInt16(src, ref seeker);

        var celLengths = new int[celTotal];
        for (var c = 0; c < celTotal; c++)
        {
            celLengths[c] = ReadUInt16(src, cellLengthsIndex + (2 * c));
        }

        var writer = 0;
        dest[writer++] = (byte)loopHeaders;
        dest[writer++] = ViewHeaderColors8Bit;
        WriteUInt16(dest, writer, (ushort)loopMask);
        writer += 2;
        WriteUInt16(dest, writer, (ushort)unknown);
        writer += 2;
        WriteUInt16(dest, writer, (ushort)paletteOffset);
        writer += 2;

        var loopOffsetIndex = writer;
        writer += 2 * loopHeaders;

        var celCountLength = Math.Max((int)loopHeaderPresence, 1);
        Span<byte> celCounts = stackalloc byte[celCountLength];
        new ReadOnlySpan<byte>(src, seeker, loopHeaderPresence).CopyTo(celCounts);
        seeker += loopHeaderPresence;

        var loopBit = 1;
        var celIndex = 0;
        var loopIndexInCounts = 0;
        var lastLoopOffset = -1;
        var celPositions = new int[celTotal];

        var rlePointerStart = cellLengthsIndex + (2 * celTotal);
        var pixelPointer = rlePointerStart;

        for (var loop = 0; loop < loopHeaders; loop++)
        {
            var loopMissing = (loopMask & loopBit) != 0;
            if (loopMissing)
            {
                if (lastLoopOffset < 0)
                {
                    lastLoopOffset = 0;
                }

                WriteUInt16(dest, loopOffsetIndex, (ushort)lastLoopOffset);
                loopOffsetIndex += 2;
            }
            else
            {
                lastLoopOffset = writer;
                WriteUInt16(dest, loopOffsetIndex, (ushort)lastLoopOffset);
                loopOffsetIndex += 2;

                var celCount = loopIndexInCounts < celCounts.Length ? celCounts[loopIndexInCounts] : (byte)0;
                loopIndexInCounts++;

                WriteUInt16(dest, writer, celCount);
                writer += 2;
                WriteUInt16(dest, writer, 0);
                writer += 2;

                var celDataOffset = writer + (2 * celCount);
                for (var c = 0; c < celCount; c++)
                {
                    WriteUInt16(dest, writer, (ushort)celDataOffset);
                    writer += 2;
                    celPositions[celIndex + c] = celDataOffset;
                    celDataOffset += 8 + celLengths[celIndex + c];
                }

                BuildCelHeaders(src, ref seeker, dest, ref writer, celIndex, celLengths, celCount);
                celIndex += celCount;
            }

            loopBit <<= 1;
        }

        if (celIndex != celTotal)
        {
            throw new InvalidDataException($"View reorder generated {celIndex} cel headers, expected {celTotal}.");
        }

        for (var c = 0; c < celTotal; c++)
        {
            pixelPointer += GetRleSize(src, pixelPointer, celLengths[c]);
        }

        var rlePointer = rlePointerStart;
        var pixelDataPointer = pixelPointer;

        for (var c = 0; c < celTotal; c++)
        {
            var outputOffset = celPositions[c] + 8;
            DecodeRle(src, ref rlePointer, src, ref pixelDataPointer, dest.Slice(outputOffset, celLengths[c]), celLengths[c]);
        }

        if (paletteOffset != 0)
        {
            var palWriter = writer;
            dest[palWriter++] = (byte)'P';
            dest[palWriter++] = (byte)'A';
            dest[palWriter++] = (byte)'L';

            for (var i = 0; i < 256; i++)
            {
                dest[palWriter++] = (byte)i;
            }

            seeker -= 4;
            const int palBytes = (4 * 256) + 4;
            new ReadOnlySpan<byte>(src, seeker, palBytes).CopyTo(dest.Slice(palWriter, palBytes));
        }
    }

    private static void DecodeRle(ReadOnlySpan<byte> rleSource, ref int rleOffset, ReadOnlySpan<byte> pixelSource, ref int pixelOffset, Span<byte> output, int expectedLength)
    {
        var position = 0;
        while (position < expectedLength)
        {
            var nextByte = rleSource[rleOffset++];
            output[position++] = nextByte;

            switch (nextByte & 0xC0)
            {
                case 0x40:
                case 0x00:
                    var count = nextByte;
                    if (count > 0)
                    {
                        pixelSource.Slice(pixelOffset, count).CopyTo(output.Slice(position, count));
                        pixelOffset += count;
                        position += count;
                    }
                    break;
                case 0x80:
                    output[position++] = pixelSource[pixelOffset++];
                    break;
                case 0xC0:
                    break;
            }
        }
    }

    private static void DecodeRle(byte[] rleSource, ref int rleOffset, byte[] pixelSource, ref int pixelOffset, Span<byte> output, int expectedLength)
        => DecodeRle(rleSource.AsSpan(), ref rleOffset, pixelSource.AsSpan(), ref pixelOffset, output, expectedLength);

    private static int GetRleSize(byte[] data, int offset, int decompressedLength)
    {
        var position = 0;
        var size = 0;
        var index = offset;

        while (position < decompressedLength)
        {
            var next = data[index++];
            position++;
            size++;

            switch (next & 0xC0)
            {
                case 0x40:
                case 0x00:
                    position += next;
                    break;
                case 0x80:
                    position++;
                    break;
            }
        }

        return size;
    }

    private static void BuildCelHeaders(byte[] source, ref int seeker, Span<byte> destination, ref int writer, int celIndex, int[] celLengths, int count)
    {
        for (var i = 0; i < count; i++)
        {
            new ReadOnlySpan<byte>(source, seeker, 6).CopyTo(destination.Slice(writer, 6));
            seeker += 6;
            writer += 6;

            var width = source[seeker++];
            WriteUInt16(destination, writer, (ushort)width);
            writer += 2;

            writer += celLengths[celIndex + i];
        }
    }

    private static void CopyBytes(byte[] source, ref int seeker, Span<byte> destination, ref int writer, int length)
    {
        if (length <= 0)
        {
            return;
        }

        new ReadOnlySpan<byte>(source, seeker, length).CopyTo(destination.Slice(writer, length));
        seeker += length;
        writer += length;
    }

    private static void CopyBytes(byte[] source, ref int seeker, Span<byte> destination, int offset, int length)
    {
        if (length <= 0)
        {
            return;
        }

        new ReadOnlySpan<byte>(source, seeker, length).CopyTo(destination.Slice(offset, length));
        seeker += length;
    }

    private static ushort ReadUInt16(byte[] data, ref int offset)
    {
        var value = ReadUInt16(data, offset);
        offset += 2;
        return value;
    }

    private static ushort ReadUInt16(byte[] data, int offset)
        => BinaryPrimitives.ReadUInt16LittleEndian(new ReadOnlySpan<byte>(data, offset, 2));

    private static void WriteUInt16(Span<byte> destination, int offset, ushort value)
        => BinaryPrimitives.WriteUInt16LittleEndian(destination.Slice(offset, 2), value);

    private static void WriteUInt32(Span<byte> destination, int offset, uint value)
        => BinaryPrimitives.WriteUInt32LittleEndian(destination.Slice(offset, 4), value);

    private static void EnsureCapacity(int actualLength, int minimumLength)
    {
        if (actualLength < minimumLength)
        {
            throw new InvalidDataException("Destination buffer too small for PIC reorder operation.");
        }
    }
}
