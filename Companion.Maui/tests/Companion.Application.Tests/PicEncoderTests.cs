using System;
using Companion.Domain.Resources.Pic;
using Xunit;

namespace Companion.Application.Tests;

public class PicEncoderTests
{
    [Fact]
    public void EncodeReturnsCopyOfOriginalPayload()
    {
        var encoder = new PicEncoder();
        var document = new PicDocument(320, 190, Array.Empty<PicCommand>(), Array.Empty<PicStateSnapshot>(), new PicStateSnapshot(PicStateFlags.None, 0, 0, 0, 0, 0, 0, 0, 0, 0, Array.Empty<bool>(), Array.Empty<byte[]>(), Array.Empty<byte>(), Array.Empty<ushort>()), Array.Empty<byte>(), Array.Empty<byte>(), Array.Empty<byte>());
        var payload = new byte[] { 1, 2, 3 };

        var result = encoder.Encode(document, payload);

        Assert.Equal(payload, result);
        Assert.False(ReferenceEquals(payload, result));
    }
}
