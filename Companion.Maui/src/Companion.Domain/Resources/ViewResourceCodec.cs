using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class ViewResourceCodec : IResourceCodec
{
    private const string HeaderKey = "ViewHeader";

    public ResourceType ResourceType => ResourceType.View;

    public DecodedResource Decode(ResourcePackage package)
    {
        var headerBytes = SliceHeader(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [HeaderKey] = headerBytes,
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
            throw new InvalidOperationException("View payload cannot be empty.");
        }
    }

    private static byte[] SliceHeader(byte[] payload)
    {
        var length = Math.Min(64, payload.Length);
        var header = new byte[length];
        Array.Copy(payload, 0, header, 0, length);
        return header;
    }
}
