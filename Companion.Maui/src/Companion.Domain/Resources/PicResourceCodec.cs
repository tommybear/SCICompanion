using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class PicResourceCodec : IResourceCodec
{
    public ResourceType ResourceType => ResourceType.Pic;

    public DecodedResource Decode(ResourcePackage package)
    {
        var metadata = new Dictionary<string, object?>
        {
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
            throw new InvalidOperationException("PIC resource payload cannot be empty.");
        }
    }
}
