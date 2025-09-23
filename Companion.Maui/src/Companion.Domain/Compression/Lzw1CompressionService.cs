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
        var destIndex = 0;

        while (true)
        {
            switch (state.DecryptStage)
            {
                case 0:
                case 1:
                    state.BitString = (int)state.ReadBits(state.NumBits);
                    if (state.BitString == 0x101)
                    {
                        state.DecryptStage = 4;
                        goto case 4;
                    }

                    if (state.DecryptStage == 0)
                    {
                        state.DecryptStage = 1;
                        state.LastBits = state.BitString;
                        WriteByte(ref destIndex, output, (byte)(state.BitString & 0xFF));
                        state.LastChar = (byte)(state.BitString & 0xFF);
                        if (destIndex >= expectedLength)
                        {
                            return output;
                        }
                        continue;
                    }

                    if (state.BitString == 0x100)
                    {
                        state.ResetAfterClear();
                        continue;
                    }

                    var token = state.BitString;
                    if (token >= state.CurrentToken)
                    {
                        token = state.LastBits;
                        state.PushToStack(state.LastChar);
                    }

                    while (token > 0xFF && token < state.TokenTable.Length)
                    {
                        var entry = state.TokenTable[token];
                        state.PushToStack(entry.Data);
                        token = entry.Next;
                    }

                    state.LastChar = (byte)(token & 0xFF);
                    state.PushToStack(state.LastChar);
                    state.DecryptStage = 2;
                    goto case 2;

                case 2:
                    while (state.StackPointer > 0)
                    {
                        var value = state.PopFromStack();
                        WriteByte(ref destIndex, output, value);
                        if (destIndex >= expectedLength)
                        {
                            state.DecryptStage = 2;
                            return output;
                        }
                    }

                    state.DecryptStage = 1;
                    if (state.CurrentToken <= state.EndToken)
                    {
                        state.TokenTable[state.CurrentToken].Data = state.LastChar;
                        state.TokenTable[state.CurrentToken].Next = state.LastBits;
                        state.CurrentToken++;
                        if (state.CurrentToken == state.EndToken && state.NumBits < 12)
                        {
                            state.NumBits++;
                            state.EndToken = (state.EndToken << 1) | 1;
                        }
                    }

                    state.LastBits = state.BitString;
                    continue;

                case 4:
                    return destIndex == output.Length ? output : ResizeOutput(output, destIndex);

                default:
                    throw new InvalidOperationException($"Unknown LZW_1 decrypt stage {state.DecryptStage}.");
            }
        }
    }

    private static void WriteByte(ref int index, byte[] destination, byte value)
    {
        if (index >= destination.Length)
        {
            throw new InvalidOperationException("LZW_1 decoding overflow (output index).");
        }

        destination[index++] = value;
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
        public struct Token
        {
            public byte Data;
            public int Next;
        }

        private readonly byte[] _source;

        public Token[] TokenTable { get; } = new Token[0x1004];
        public byte[] Stack { get; } = new byte[0x1014];
        public int StackPointer { get; set; }
        public byte LastChar { get; set; }
        public int NumBits { get; set; }
        public int BitString { get; set; }
        public int LastBits { get; set; }
        public int DecryptStage { get; set; }
        public int CurrentToken { get; set; }
        public int EndToken { get; set; }
        public int BitPosition { get; set; }

        public Decrypt3State(byte[] source)
        {
            _source = source;
            Initialize();
        }

        public void ResetAfterClear()
        {
            NumBits = 9;
            CurrentToken = 0x102;
            EndToken = 0x1FF;
            DecryptStage = 0;
            StackPointer = 0;
            LastChar = 0;
            LastBits = 0;
            Array.Clear(TokenTable, 0, TokenTable.Length);
        }

        public uint ReadBits(int numBits)
        {
            if (numBits == 0)
            {
                BitPosition = 0;
                return 0;
            }

            var place = BitPosition >> 3;
            uint bitstring = 0;
            for (var i = (numBits >> 3) + 1; i >= 0; i--)
            {
                var index = place + i;
                if (index < _source.Length)
                {
                    bitstring |= (uint)_source[index] << (8 * (2 - i));
                }
            }

            var shift = 24 - (BitPosition & 7) - numBits;
            if (shift > 0)
            {
                bitstring >>= shift;
            }
            bitstring &= uint.MaxValue >> (32 - numBits);
            BitPosition += numBits;
            return bitstring;
        }

        public void PushToStack(byte value)
        {
            if (StackPointer >= Stack.Length)
            {
                throw new InvalidOperationException("LZW_1 decoding stack overflow.");
            }

            Stack[StackPointer++] = value;
        }

        public byte PopFromStack()
        {
            if (StackPointer <= 0)
            {
                throw new InvalidOperationException("LZW_1 decoding stack underflow.");
            }

            return Stack[--StackPointer];
        }

        private void Initialize()
        {
            NumBits = 9;
            CurrentToken = 0x102;
            EndToken = 0x1FF;
            DecryptStage = 0;
            StackPointer = 0;
            LastChar = 0;
            BitString = 0;
            LastBits = 0;
            BitPosition = 0;
            Array.Clear(TokenTable, 0, TokenTable.Length);
        }
    }
}
