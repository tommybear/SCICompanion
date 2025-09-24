using System;

namespace Companion.Domain.Resources.Pic;

internal sealed class PicEncoder
{
    public byte[] Encode(PicDocument document, byte[] originalPayload)
    {
        if (document is null)
        {
            throw new ArgumentNullException(nameof(document));
        }

        if (originalPayload is null)
        {
            throw new ArgumentNullException(nameof(originalPayload));
        }

        var copy = new byte[originalPayload.Length];
        Array.Copy(originalPayload, copy, copy.Length);
        return copy;
    }
}
