using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class MessageResourceCodec : IResourceCodec
{
    private const string OffsetsKey = "MessageOffsets";
    private const string CompressionMethodKey = "CompressionMethod";

    public ResourceType ResourceType => ResourceType.Message;

    public DecodedResource Decode(ResourcePackage package)
    {
        var offsets = ParseOffsets(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [OffsetsKey] = offsets,
            [CompressionMethodKey] = package.Header.CompressionMethod
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
            throw new InvalidOperationException("Message payload cannot be empty.");
        }
    }

    private static IReadOnlyList<int> ParseOffsets(byte[] payload)
    {
        var list = new List<int>();
        for (int i = 0; i + 3 < payload.Length; i += 4)
        {
            var value = payload[i] | (payload[i + 1] << 8) | (payload[i + 2] << 16) | (payload[i + 3] << 24);
            list.Add(value);
        }
        return list;
    }
}
