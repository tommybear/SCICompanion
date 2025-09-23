using System;

namespace Companion.Domain.Compression;

public sealed class Lzw1CompressionService : ICompressionService
{
    private readonly int[] _supportedMethods;

    public Lzw1CompressionService(params int[] methods)
    {
        _supportedMethods = methods.Length > 0 ? methods : new[] { 2 };
    }

    public bool SupportsMethod(int method) => Array.IndexOf(_supportedMethods, method) >= 0;

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported by LZW_1 service.");
        }

        return DecompressInternal(data, expectedLength);
    }

    private static byte[] DecompressInternal(byte[] data, int expectedLength)
    {
        if (expectedLength < 0)
        {
            throw new ArgumentOutOfRangeException(nameof(expectedLength), "Expected length must be non-negative for LZW_1 decoding.");
        }

        if (expectedLength == 0)
        {
            return Array.Empty<byte>();
        }

        var state = new Decrypt3State(data);
        var output = new byte[expectedLength];
        var written = state.Decompress(output);
        return written == output.Length ? output : ResizeOutput(output, written);
    }

    private static byte[] ResizeOutput(byte[] buffer, int length)
    {
        if (length == buffer.Length)
        {
            return buffer;
        }

        var resized = new byte[length];
        Array.Copy(buffer, resized, length);
        return resized;
    }

    private sealed class Decrypt3State
    {
        private struct Token
        {
            public byte Data;
            public short Next;
        }

        private readonly byte[] _source;
        private readonly Token[] _tokens = new Token[0x1004];
        private readonly byte[] _stack = new byte[0x1014];
        private short _stackPtr;
        private byte _lastChar;
        private ushort _numBits;
        private ushort _bitString;
        private ushort _lastBits;
        private short _curToken;
        private short _endToken;
        private ushort _decryptStart;
        private int _whichBit;

        public Decrypt3State(byte[] source)
        {
            _source = source;
            Reset();
        }

        public int Decompress(Span<byte> destination)
        {
            var lengthRemaining = destination.Length;
            var destIndex = 0;

            while (lengthRemaining >= 0)
            {
                switch (_decryptStart)
                {
                    case 0:
                    case 1:
                        _bitString = (ushort)Gbits(_numBits);
                        if (_bitString == 0x101)
                        {
                            _decryptStart = 4;
                            goto case 4;
                        }

                        if (_decryptStart == 0)
                        {
                            _decryptStart = 1;
                            _lastBits = _bitString;
                            destination[destIndex++] = _lastChar = (byte)(_bitString & 0xFF);
                            lengthRemaining--;
                            if (lengthRemaining > 0)
                            {
                                continue;
                            }
                            return destIndex;
                        }

                        if (_bitString == 0x100)
                        {
                            ResetAfterClear();
                            continue;
                        }

                        var token = (short)_bitString;
                        if (token >= _curToken)
                        {
                            token = (short)_lastBits;
                            PushToStack(_lastChar);
                        }

                        while (token > 0xFF && token < _tokens.Length)
                        {
                            PushToStack(_tokens[token].Data);
                            token = _tokens[token].Next;
                        }

                        _lastChar = (byte)(token & 0xFF);
                        PushToStack(_lastChar);
                        _decryptStart = 2;
                        goto case 2;

                    case 2:
                        while (_stackPtr > 0)
                        {
                            destination[destIndex++] = PopFromStack();
                            lengthRemaining--;
                            if (lengthRemaining == 0)
                            {
                                _decryptStart = 2;
                                return destIndex;
                            }
                        }

                        _decryptStart = 1;
                        if (_curToken <= _endToken)
                        {
                            _tokens[_curToken].Data = _lastChar;
                            _tokens[_curToken].Next = (short)_lastBits;
                            _curToken++;
                            if (_curToken == _endToken && _numBits < 12)
                            {
                                _numBits++;
                                _endToken = (short)((_endToken << 1) | 1);
                            }
                        }

                        _lastBits = _bitString;
                        continue;

                    case 4:
                        return destIndex;

                    default:
                        throw new InvalidOperationException($"Unexpected LZW_1 decrypt stage {_decryptStart}.");
                }
            }

            return destIndex;
        }

        private void Reset()
        {
            _stackPtr = 0;
            _lastChar = 0;
            _numBits = 9;
            _bitString = 0;
            _lastBits = 0;
            _curToken = 0x102;
            _endToken = 0x1FF;
            _decryptStart = 0;
            _whichBit = 0;
            Array.Clear(_tokens);
        }

        private void ResetAfterClear()
        {
            _numBits = 9;
            _curToken = 0x102;
            _endToken = 0x1FF;
            _decryptStart = 0;
            _stackPtr = 0;
        }

        private uint Gbits(int numBits)
        {
            if (numBits == 0)
            {
                _whichBit = 0;
                return 0;
            }

            var place = _whichBit >> 3;
            uint bitstring = 0;
            for (var i = (numBits >> 3) + 1; i >= 0; i--)
            {
                var index = place + i;
                if (index < _source.Length)
                {
                    bitstring |= (uint)_source[index] << (8 * (2 - i));
                }
            }

            var shift = 24 - (_whichBit & 7) - numBits;
            if (shift > 0)
            {
                bitstring >>= shift;
            }

            bitstring &= uint.MaxValue >> (32 - numBits);
            _whichBit += numBits;
            return bitstring;
        }

        private void PushToStack(byte value)
        {
            if (_stackPtr >= _stack.Length)
            {
                throw new InvalidOperationException("LZW_1 decoding stack overflow.");
            }

            _stack[_stackPtr++] = value;
        }

        private byte PopFromStack()
        {
            if (_stackPtr <= 0)
            {
                throw new InvalidOperationException("LZW_1 decoding stack underflow.");
            }

            return _stack[--_stackPtr];
        }
    }
}
