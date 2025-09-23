using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class SoundResourceCodec : IResourceCodec
{
    private const string HeaderMetadataKey = "SoundHeader";

    public ResourceType ResourceType => ResourceType.Sound;

    public DecodedResource Decode(ResourcePackage package)
    {
        var header = ParseHeader(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [HeaderMetadataKey] = header,
            ["CompressionMethod"] = package.Header.CompressionMethod
        };
        return new DecodedResource(package, package.Body, metadata);
    }

    public ResourcePackage Encode(DecodedResource resource)
    {
        var header = resource.Header with
        {
            CompressedLength = resource.Payload.Length
        };
        return new ResourcePackage(resource.Version, header, resource.Payload);
    }

    public void Validate(DecodedResource resource)
    {
        if (resource.Payload.Length == 0)
        {
            throw new InvalidOperationException("Sound payload cannot be empty.");
        }
    }

    private static byte[] ParseHeader(byte[] payload)
    {
        var length = Math.Min(32, payload.Length);
        var header = new byte[length];
        Array.Copy(payload, 0, header, 0, length);
        return header;
    }
}
