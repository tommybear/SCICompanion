using System;
using System.IO;

namespace Companion.Domain.Compression;

public sealed class StacCompressionService : ICompressionService
{
    private readonly int[] _supportedMethods;

    public StacCompressionService(params int[] methods)
    {
        _supportedMethods = methods.Length > 0 ? methods : new[] { 32 };
    }

    public bool SupportsMethod(int method) => Array.IndexOf(_supportedMethods, method) >= 0;

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported by STAC service.");
        }

        if (expectedLength < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(expectedLength));
        }

        var decoder = new StacDecoder(data, expectedLength);
        return decoder.Decode();
    }

    private sealed class StacDecoder
    {
        private readonly byte[] _source;
        private readonly int _expectedLength;
        private readonly byte[] _destination;

        private int _sourceIndex;
        private int _destinationIndex;
        private int _currentByte;
        private int _bitsRemaining;

        public StacDecoder(byte[] source, int expectedLength)
        {
            _source = source ?? throw new ArgumentNullException(nameof(source));
            _expectedLength = expectedLength;
            _destination = expectedLength == 0 ? Array.Empty<byte>() : new byte[expectedLength];
            _sourceIndex = 0;
            _destinationIndex = 0;
            _bitsRemaining = 0;
            _currentByte = 0;
        }

        public byte[] Decode()
        {
            if (_expectedLength == 0)
            {
                return Array.Empty<byte>();
            }

            int offset;
            while (_destinationIndex < _expectedLength)
            {
                if (ReadBit() == 1)
                {
                    if (ReadBit() == 1)
                    {
                        offset = ReadBits(7);
                        if (offset == 0)
                        {
                            break; // End marker
                        }
                        var length = ReadCompressedLength();
                        CopyFromHistory(offset, length);
                    }
                    else
                    {
                        offset = ReadBits(11);
                        var length = ReadCompressedLength();
                        CopyFromHistory(offset, length);
                    }
                }
                else
                {
                    WriteByte((byte)ReadBits(8));
                }
            }

            if (_destinationIndex != _expectedLength)
            {
                throw new InvalidDataException($"STAC decompression wrote {_destinationIndex} bytes, expected {_expectedLength}.");
            }

            return _destination;
        }

        private void CopyFromHistory(int offset, int length)
        {
            if (offset <= 0)
            {
                throw new InvalidDataException($"STAC copy used non-positive offset {offset}.");
            }

            if (length <= 0)
            {
                throw new InvalidDataException("STAC copy used non-positive length.");
            }

            if (offset > _destinationIndex)
            {
                throw new InvalidDataException($"STAC copy references offset {offset} beyond written {_destinationIndex} bytes.");
            }

            if (_destinationIndex + length > _expectedLength)
            {
                throw new InvalidDataException("STAC copy exceeds destination buffer.");
            }

            var sourcePosition = _destinationIndex - offset;
            while (length-- > 0)
            {
                WriteByte(_destination[sourcePosition++]);
            }
        }

        private int ReadCompressedLength()
        {
            var value = ReadBits(2);
            return value switch
            {
                0 => 2,
                1 => 3,
                2 => 4,
                _ => ReadExtendedLength()
            };
        }

        private int ReadExtendedLength()
        {
            var next = ReadBits(2);
            return next switch
            {
                0 => 5,
                1 => 6,
                2 => 7,
                _ => ReadVariableLength()
            };
        }

        private int ReadVariableLength()
        {
            var length = 8;
            int nibble;
            do
            {
                nibble = ReadBits(4);
                length += nibble;
            } while (nibble == 0xF);

            return length;
        }

        private int ReadBits(int count)
        {
            var value = 0;
            for (var i = 0; i < count; i++)
            {
                value = (value << 1) | ReadBit();
            }
            return value;
        }

        private int ReadBit()
        {
            if (_bitsRemaining == 0)
            {
                if (_sourceIndex >= _source.Length)
                {
                    throw new InvalidDataException("Unexpected end of STAC stream.");
                }

                _currentByte = _source[_sourceIndex++];
                _bitsRemaining = 8;
            }

            var bit = (_currentByte >> 7) & 1;
            _currentByte = (_currentByte << 1) & 0xFF;
            _bitsRemaining--;
            return bit;
        }

        private void WriteByte(byte value)
        {
            if (_destinationIndex >= _expectedLength)
            {
                throw new InvalidDataException("STAC decompressor attempted to write past the end of the destination buffer.");
            }

            _destination[_destinationIndex++] = value;
        }
    }
}
