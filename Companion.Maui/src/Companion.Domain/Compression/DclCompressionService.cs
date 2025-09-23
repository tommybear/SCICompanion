using System;
using System.IO;

namespace Companion.Domain.Compression;

public sealed class DclCompressionService : ICompressionService
{
    private readonly int[] _supportedMethods;

    public DclCompressionService(params int[] methods)
    {
        _supportedMethods = methods.Length > 0 ? methods : new[] { 8, 18, 19, 20 };
    }

    public bool SupportsMethod(int method) => Array.IndexOf(_supportedMethods, method) >= 0;

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported by DCL service.");
        }

        var decoder = new DclDecoder(data, expectedLength);
        return decoder.Decode();
    }

    private sealed class DclDecoder
    {
        private const int HuffmanLeaf = unchecked((int)0x40000000);

        private readonly byte[] _source;
        private readonly int _sourceLength;
        private readonly byte[] _destination;
        private readonly int _expectedLength;

        private int _sourceIndex;
        private int _destinationIndex;
        private uint _bitBuffer;
        private int _bitCount;

        public DclDecoder(byte[] source, int expectedLength)
        {
            _source = source ?? throw new ArgumentNullException(nameof(source));
            _sourceLength = source.Length;
            _expectedLength = expectedLength;
            _destination = new byte[expectedLength];
            _sourceIndex = 0;
            _destinationIndex = 0;
            _bitBuffer = 0;
            _bitCount = 0;
        }

        public byte[] Decode()
        {
            if (_expectedLength == 0)
            {
                return Array.Empty<byte>();
            }

            var mode = ReadByte();
            var lengthParam = ReadByte();

            if (mode is not 0 and not 1)
            {
                throw new InvalidDataException($"Unsupported DCL mode {mode:X2}.");
            }

            if (lengthParam < 0)
            {
                throw new InvalidDataException("Invalid DCL length parameter.");
            }

            while (_destinationIndex < _expectedLength)
            {
                var isCopy = ReadBits(1);
                if (isCopy != 0)
                {
                    var lengthSymbol = HuffmanLookup(LengthTree);
                    int runLength;
                    if (lengthSymbol < 8)
                    {
                        runLength = lengthSymbol + 2;
                    }
                    else
                    {
                        var extraBits = lengthSymbol - 7;
                        runLength = 8 + (1 << extraBits) + ReadBits(extraBits);
                    }

                    var distanceSymbol = HuffmanLookup(DistanceTree);
                    int distance = runLength == 2
                        ? (distanceSymbol << 2) | ReadBits(2)
                        : (distanceSymbol << lengthParam) | ReadBits(lengthParam);
                    distance += 1;

                    CopyFromHistory(distance, runLength);
                }
                else
                {
                    int value = mode == 1 ? HuffmanLookup(AsciiTree) : ReadByte();
                    WriteByte((byte)(value & 0xFF));
                }
            }

            if (_destinationIndex != _expectedLength)
            {
                throw new InvalidDataException($"DCL decompression wrote {_destinationIndex} bytes, expected {_expectedLength}.");
            }

            return _destination;
        }

        private void CopyFromHistory(int distance, int length)
        {
            if (distance <= 0 || distance > _destinationIndex)
            {
                throw new InvalidDataException($"DCL copy references distance {distance} with {_destinationIndex} bytes written.");
            }

            var available = _destination.Length - _destinationIndex;
            if (length > available)
            {
                throw new InvalidDataException("DCL copy exceeds destination buffer.");
            }

            while (length > 0)
            {
                var chunk = Math.Min(length, distance);
                var sourcePos = _destinationIndex - distance;
                for (var i = 0; i < chunk; i++)
                {
                    WriteByte(_destination[sourcePos + i]);
                }
                length -= chunk;
                distance += chunk;
            }
        }

        private int HuffmanLookup(ReadOnlySpan<int> tree)
        {
            var index = 0;
            while ((tree[index] & HuffmanLeaf) == 0)
            {
                var entry = tree[index];
                var bit = ReadBits(1);
                var left = entry >> 12;
                var right = entry & 0xFFF;
                index = bit != 0 ? right : left;
            }

            return tree[index] & 0xFFFF;
        }

        private int ReadBits(int count)
        {
            if (count == 0)
            {
                return 0;
            }

            while (_bitCount < count)
            {
                if (_sourceIndex >= _sourceLength)
                {
                    throw new InvalidDataException("Unexpected end of DCL stream.");
                }

                _bitBuffer |= (uint)_source[_sourceIndex++] << _bitCount;
                _bitCount += 8;
            }

            uint mask = count == 32 ? uint.MaxValue : (1u << count) - 1u;
            var value = (int)(_bitBuffer & mask);
            _bitBuffer >>= count;
            _bitCount -= count;
            return value;
        }

        private byte ReadByte() => (byte)ReadBits(8);

        private void WriteByte(byte value)
        {
            if (_destinationIndex >= _expectedLength)
            {
                throw new InvalidDataException("DCL decompressor attempted to write past the end of the destination buffer.");
            }

            _destination[_destinationIndex++] = value;
        }

        private static readonly int[] LengthTree =
        {
            0x00001002, 0x00003004, 0x00005006, 0x00007008, 0x0000900A, 0x0000B00C,
            0x40000001, 0x0000D00E, 0x0000F010, 0x00011012, 0x40000003, 0x40000002,
            0x40000000, 0x00013014, 0x00015016, 0x00017018, 0x40000006, 0x40000005,
            0x40000004, 0x0001901A, 0x0001B01C, 0x4000000A, 0x40000009, 0x40000008,
            0x40000007, 0x0001D01E, 0x4000000D, 0x4000000C, 0x4000000B, 0x4000000F,
            0x4000000E, 0x00000000,
        };

        private static readonly int[] DistanceTree =
        {
            0x00001002, 0x00003004, 0x00005006, 0x00007008, 0x0000900A, 0x0000B00C,
            0x40000000, 0x0000D00E, 0x0000F010, 0x00011012, 0x00013014, 0x00015016,
            0x00017018, 0x0001901A, 0x0001B01C, 0x0001D01E, 0x0001F020, 0x00021022,
            0x00023024, 0x00025026, 0x00027028, 0x0002902A, 0x0002B02C, 0x40000002,
            0x40000001, 0x0002D02E, 0x0002F030, 0x00031032, 0x00033034, 0x00035036,
            0x00037038, 0x0003903A, 0x0003B03C, 0x0003D03E, 0x0003F040, 0x00041042,
            0x00043044, 0x00045046, 0x00047048, 0x0004904A, 0x0004B04C, 0x40000006,
            0x40000005, 0x40000004, 0x40000003, 0x0004D04E, 0x0004F050, 0x00051052,
            0x00053054, 0x00055056, 0x00057058, 0x0005905A, 0x0005B05C, 0x0005D05E,
            0x0005F060, 0x00061062, 0x00063064, 0x00065066, 0x00067068, 0x0006906A,
            0x0006B06C, 0x0006D06E, 0x40000015, 0x40000014, 0x40000013, 0x40000012,
            0x40000011, 0x40000010, 0x4000000F, 0x4000000E, 0x4000000D, 0x4000000C,
            0x4000000B, 0x4000000A, 0x40000009, 0x40000008, 0x40000007, 0x0006F070,
            0x00071072, 0x00073074, 0x00075076, 0x00077078, 0x0007907A, 0x0007B07C,
            0x0007D07E, 0x4000002F, 0x4000002E, 0x4000002D, 0x4000002C, 0x4000002B,
            0x4000002A, 0x40000029, 0x40000028, 0x40000027, 0x40000026, 0x40000025,
            0x40000024, 0x40000023, 0x40000022, 0x40000021, 0x40000020, 0x4000001F,
            0x4000001E, 0x4000001D, 0x4000001C, 0x4000001B, 0x4000001A, 0x40000019,
            0x40000018, 0x40000017, 0x40000016, 0x4000003F, 0x4000003E, 0x4000003D,
            0x4000003C, 0x4000003B, 0x4000003A, 0x40000039, 0x40000038, 0x40000037,
            0x40000036, 0x40000035, 0x40000034, 0x40000033, 0x40000032, 0x40000031,
            0x40000030, 0x00000000,
        };

        private static readonly int[] AsciiTree =
        {
            0x00001002, 0x00003004, 0x00005006, 0x00007008, 0x0000900A, 0x0000B00C,
            0x0000D00E, 0x0000F010, 0x00011012, 0x00013014, 0x00015016, 0x00017018,
            0x0001901A, 0x0001B01C, 0x0001D01E, 0x0001F020, 0x00021022, 0x00023024,
            0x00025026, 0x00027028, 0x0002902A, 0x0002B02C, 0x0002D02E, 0x0002F030,
            0x00031032, 0x00033034, 0x00035036, 0x00037038, 0x0003903A, 0x0003B03C,
            0x40000020, 0x0003D03E, 0x0003F040, 0x00041042, 0x00043044, 0x00045046,
            0x00047048, 0x0004904A, 0x0004B04C, 0x0004D04E, 0x0004F050, 0x00051052,
            0x00053054, 0x00055056, 0x00057058, 0x0005905A, 0x0005B05C, 0x0005D05E,
            0x0005F060, 0x00061062, 0x40000075, 0x40000074, 0x40000073, 0x40000072,
            0x4000006F, 0x4000006E, 0x4000006C, 0x40000069, 0x40000065, 0x40000061,
            0x40000045, 0x00063064, 0x00065066, 0x00067068, 0x0006906A, 0x0006B06C,
            0x0006D06E, 0x0006F070, 0x00071072, 0x00073074, 0x00075076, 0x00077078,
            0x0007907A, 0x0007B07C, 0x0007D07E, 0x0007F080, 0x00081082, 0x00083084,
            0x00085086, 0x40000070, 0x4000006D, 0x40000068, 0x40000067, 0x40000066,
            0x40000064, 0x40000063, 0x40000062, 0x40000054, 0x40000053, 0x40000052,
            0x4000004F, 0x4000004E, 0x4000004C, 0x40000049, 0x40000044, 0x40000043,
            0x40000041, 0x40000031, 0x4000002D, 0x00087088, 0x0008908A, 0x0008B08C,
            0x0008D08E, 0x0008F090, 0x00091092, 0x00093094, 0x00095096, 0x00097098,
            0x0009909A, 0x0009B09C, 0x0009D09E, 0x0009F0A0, 0x000A10A2, 0x000A30A4,
            0x40000077, 0x4000006B, 0x40000055, 0x40000050, 0x4000004D, 0x40000046,
            0x40000042, 0x4000003D, 0x40000038, 0x40000037, 0x40000035, 0x40000034,
            0x40000033, 0x40000032, 0x40000030, 0x4000002E, 0x4000002C, 0x40000029,
            0x40000028, 0x4000000D, 0x4000000A, 0x000A50A6, 0x000A70A8, 0x000A90AA,
            0x000AB0AC, 0x000AD0AE, 0x000AF0B0, 0x000B10B2, 0x000B30B4, 0x000B50B6,
            0x000B70B8, 0x000B90BA, 0x000BB0BC, 0x000BD0BE, 0x000BF0C0, 0x40000079,
            0x40000078, 0x40000076, 0x4000005F, 0x4000005B, 0x40000057, 0x40000048,
            0x40000047, 0x4000003A, 0x40000039, 0x40000036, 0x4000002F, 0x4000002A,
            0x40000027, 0x40000022, 0x40000009, 0x000C10C2, 0x000C30C4, 0x000C50C6,
            0x000C70C8, 0x000C90CA, 0x000CB0CC, 0x000CD0CE, 0x000CF0D0, 0x000D10D2,
            0x000D30D4, 0x000D50D6, 0x000D70D8, 0x000D90DA, 0x000DB0DC, 0x000DD0DE,
            0x000DF0E0, 0x000E10E2, 0x000E30E4, 0x000E50E6, 0x000E70E8, 0x000E90EA,
            0x4000005D, 0x40000059, 0x40000058, 0x40000056, 0x4000004B, 0x4000003E,
            0x4000002B, 0x000EB0EC, 0x000ED0EE, 0x000EF0F0, 0x000F10F2, 0x000F30F4,
            0x000F50F6, 0x000F70F8, 0x000F90FA, 0x000FB0FC, 0x000FD0FE, 0x000FF100,
            0x00101102, 0x00103104, 0x00105106, 0x00107108, 0x0010910A, 0x0010B10C,
            0x0010D10E, 0x0010F110, 0x00111112, 0x00113114, 0x00115116, 0x00117118,
            0x0011911A, 0x0011B11C, 0x0011D11E, 0x0011F120, 0x00121122, 0x00123124,
            0x00125126, 0x00127128, 0x0012912A, 0x0012B12C, 0x0012D12E, 0x0012F130,
            0x00131132, 0x00133134, 0x4000007A, 0x40000071, 0x40000026, 0x40000024,
            0x40000021, 0x00135136, 0x00137138, 0x0013913A, 0x0013B13C, 0x0013D13E,
            0x0013F140, 0x00141142, 0x00143144, 0x00145146, 0x00147148, 0x0014914A,
            0x0014B14C, 0x0014D14E, 0x0014F150, 0x00151152, 0x00153154, 0x00155156,
            0x00157158, 0x0015915A, 0x0015B15C, 0x0015D15E, 0x0015F160, 0x00161162,
            0x00163164, 0x00165166, 0x00167168, 0x0016916A, 0x0016B16C, 0x0016D16E,
            0x0016F170, 0x00171172, 0x00173174, 0x00175176, 0x00177178, 0x0017917A,
            0x0017B17C, 0x0017D17E, 0x0017F180, 0x00181182, 0x00183184, 0x00185186,
            0x00187188, 0x0018918A, 0x0018B18C, 0x0018D18E, 0x0018F190, 0x00191192,
            0x00193194, 0x00195196, 0x00197198, 0x0019919A, 0x0019B19C, 0x0019D19E,
            0x0019F1A0, 0x001A11A2, 0x001A31A4, 0x001A51A6, 0x001A71A8, 0x001A91AA,
            0x001AB1AC, 0x001AD1AE, 0x001AF1B0, 0x001B11B2, 0x001B31B4, 0x4000007C,
            0x4000007B, 0x4000006A, 0x4000005C, 0x4000005A, 0x40000051, 0x4000004A,
            0x4000003F, 0x4000003C, 0x40000000, 0x001B51B6, 0x001B71B8, 0x001B91BA,
            0x001BB1BC, 0x001BD1BE, 0x001BF1C0, 0x001C11C2, 0x001C31C4, 0x001C51C6,
            0x001C71C8, 0x001C91CA, 0x001CB1CC, 0x001CD1CE, 0x001CF1D0, 0x001D11D2,
            0x001D31D4, 0x001D51D6, 0x001D71D8, 0x001D91DA, 0x001DB1DC, 0x001DD1DE,
            0x001DF1E0, 0x001E11E2, 0x001E31E4, 0x001E51E6, 0x001E71E8, 0x001E91EA,
            0x001EB1EC, 0x001ED1EE, 0x001EF1F0, 0x001F11F2, 0x001F31F4, 0x001F51F6,
            0x001F71F8, 0x001F91FA, 0x001FB1FC, 0x001FD1FE, 0x400000F4, 0x400000F3,
            0x400000F2, 0x400000EE, 0x400000E9, 0x400000E5, 0x400000E1, 0x400000DF,
            0x400000DE, 0x400000DD, 0x400000DC, 0x400000DB, 0x400000DA, 0x400000D9,
            0x400000D8, 0x400000D7, 0x400000D6, 0x400000D5, 0x400000D4, 0x400000D3,
            0x400000D2, 0x400000D1, 0x400000D0, 0x400000CF, 0x400000CE, 0x400000CD,
            0x400000CC, 0x400000CB, 0x400000CA, 0x400000C9, 0x400000C8, 0x400000C7,
            0x400000C6, 0x400000C5, 0x400000C4, 0x400000C3, 0x400000C2, 0x400000C1,
            0x400000C0, 0x400000BF, 0x400000BE, 0x400000BD, 0x400000BC, 0x400000BB,
            0x400000BA, 0x400000B9, 0x400000B8, 0x400000B7, 0x400000B6, 0x400000B5,
            0x400000B4, 0x400000B3, 0x400000B2, 0x400000B1, 0x400000B0, 0x4000007F,
            0x4000007E, 0x4000007D, 0x40000060, 0x4000005E, 0x40000040, 0x4000003B,
            0x40000025, 0x40000023, 0x4000001F, 0x4000001E, 0x4000001D, 0x4000001C,
            0x4000001B, 0x40000019, 0x40000018, 0x40000017, 0x40000016, 0x40000015,
            0x40000014, 0x40000013, 0x40000012, 0x40000011, 0x40000010, 0x4000000F,
            0x4000000E, 0x4000000C, 0x4000000B, 0x40000008, 0x40000007, 0x40000006,
            0x40000005, 0x40000004, 0x40000003, 0x40000002, 0x40000001, 0x400000FF,
            0x400000FE, 0x400000FD, 0x400000FC, 0x400000FB, 0x400000FA, 0x400000F9,
            0x400000F8, 0x400000F7, 0x400000F6, 0x400000F5, 0x400000F1, 0x400000F0,
            0x400000EF, 0x400000ED, 0x400000EC, 0x400000EB, 0x400000EA, 0x400000E8,
            0x400000E7, 0x400000E6, 0x400000E4, 0x400000E3, 0x400000E2, 0x400000E0,
            0x400000AF, 0x400000AE, 0x400000AD, 0x400000AC, 0x400000AB, 0x400000AA,
            0x400000A9, 0x400000A8, 0x400000A7, 0x400000A6, 0x400000A5, 0x400000A4,
            0x400000A3, 0x400000A2, 0x400000A1, 0x400000A0, 0x4000009F, 0x4000009E,
            0x4000009D, 0x4000009C, 0x4000009B, 0x4000009A, 0x40000099, 0x40000098,
            0x40000097, 0x40000096, 0x40000095, 0x40000094, 0x40000093, 0x40000092,
            0x40000091, 0x40000090, 0x4000008F, 0x4000008E, 0x4000008D, 0x4000008C,
            0x4000008B, 0x4000008A, 0x40000089, 0x40000088, 0x40000087, 0x40000086,
            0x40000085, 0x40000084, 0x40000083, 0x40000082, 0x40000081, 0x40000080,
            0x4000001A,
        };
    }
}
