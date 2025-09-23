using System.Collections.Generic;
using Companion.Domain.Projects;
using Companion.Domain.Resources.Pic;

namespace Companion.Domain.Resources;

public sealed class PicResourceCodec : IResourceCodec
{
    public ResourceType ResourceType => ResourceType.Pic;

    private const string CompressionMethodKey = "CompressionMethod";
    private const string CommandsKey = "PicCommands";

    public DecodedResource Decode(ResourcePackage package)
    {
        var metadata = new Dictionary<string, object?>
        {
            [CompressionMethodKey] = package.Header.CompressionMethod,
            [CommandsKey] = PicParser.Parse(package.Body)
        };
        return new DecodedResource(package, package.Body, metadata);
    }

    public ResourcePackage Encode(DecodedResource resource)
    {
        var header = resource.Header with
        {
            CompressedLength = resource.Payload.Length
        };

        // Future work: re-encode commands from metadata if available.
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
