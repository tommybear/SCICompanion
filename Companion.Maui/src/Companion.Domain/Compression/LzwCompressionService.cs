using System;

namespace Companion.Domain.Compression;

public sealed class LzwCompressionService : ICompressionService
{
    private readonly int[] _supportedMethods;

    public LzwCompressionService(params int[] methods)
    {
        _supportedMethods = methods.Length > 0 ? methods : new[] { 1 };
    }

    public bool SupportsMethod(int method) => Array.IndexOf(_supportedMethods, method) >= 0;

    public byte[] Decompress(byte[] data, int method, int expectedLength)
    {
        if (!SupportsMethod(method))
        {
            throw new NotSupportedException($"Compression method {method} is not supported by LZW service.");
        }

        return DecompressLzw(data, expectedLength);
    }

    private static byte[] DecompressLzw(byte[] data, int expectedLength)
    {
        if (expectedLength <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(expectedLength), "Expected length must be positive for LZW decoding.");
        }

        var output = new byte[expectedLength];

        Span<int> tokenList = stackalloc int[4096];
        Span<int> tokenLengthList = stackalloc int[4096];

        int destIndex = 0;
        int tokenCounter = 0x102;
        int bitLength = 9;
        int bitMask = 0x01ff;
        int bitCounter = 0;
        int byteCounter = 0;
        int maxToken = 0x200;
        int lastBits = 0;
        int lastChar = 0;

        while (byteCounter < data.Length)
        {
            uint tokenMaker = (uint)(data[byteCounter++] >> bitCounter);
            if (byteCounter < data.Length)
            {
                tokenMaker |= (uint)(data[byteCounter] << (8 - bitCounter));
            }
            if (byteCounter + 1 < data.Length)
            {
                tokenMaker |= (uint)(data[byteCounter + 1] << (16 - bitCounter));
            }

            int bitString = (int)(tokenMaker & (uint)bitMask);

            bitCounter += bitLength - 8;
            while (bitCounter >= 8)
            {
                bitCounter -= 8;
                byteCounter++;
            }

            if (bitString == 0x101)
            {
                break; // Terminator
            }

            if (bitString == 0x100)
            {
                // Reset command
                maxToken = 0x200;
                bitLength = 9;
                bitMask = 0x01ff;
                tokenCounter = 0x102;
                continue;
            }

            int token = bitString;
            int tokenLastLength;

            if (token > 0xFF)
            {
                if (token >= tokenCounter)
                {
                    token = lastBits;
                    if (destIndex >= output.Length)
                    {
                        throw new InvalidOperationException("LZW decoding overflow (dictionary look-up).");
                    }
                    output[destIndex++] = (byte)lastChar;
                    tokenLastLength = 1;
                }
                else
                {
                    tokenLastLength = tokenLengthList[token] + 1;
                    if (destIndex + tokenLastLength > output.Length)
                    {
                        throw new InvalidOperationException("LZW decoding overflow (token expansion).");
                    }
                    int sourceIndex = tokenList[token];
                    for (int i = 0; i < tokenLastLength; i++)
                    {
                        output[destIndex] = output[sourceIndex + i];
                        destIndex++;
                    }
                    lastChar = output[destIndex - 1];
                }
            }
            else
            {
                if (destIndex >= output.Length)
                {
                    throw new InvalidOperationException("LZW decoding overflow (single byte).");
                }
                output[destIndex++] = (byte)token;
                tokenLastLength = 1;
                lastChar = token;
            }

            if (tokenCounter == maxToken)
            {
                if (bitLength < 12)
                {
                    bitLength++;
                    bitMask = (bitMask << 1) | 1;
                    maxToken <<= 1;
                }
                else
                {
                    goto SkipDictionaryInsert;
                }
            }

            tokenList[tokenCounter] = destIndex - tokenLastLength;
            tokenLengthList[tokenCounter] = tokenLastLength;
            tokenCounter++;

        SkipDictionaryInsert:
            lastBits = bitString;
        }

        if (destIndex < expectedLength)
        {
            Array.Resize(ref output, destIndex);
        }

        return output;
    }
}
