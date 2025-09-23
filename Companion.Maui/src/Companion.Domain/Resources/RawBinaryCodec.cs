using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

/// <summary>
/// Fallback codec that simply preserves raw bytes. Useful for unsupported resource types.
/// </summary>
public sealed class RawBinaryCodec : IResourceCodec
{
    public RawBinaryCodec(ResourceType type)
    {
        ResourceType = type;
    }

    public ResourceType ResourceType { get; }

    public DecodedResource Decode(Span<byte> data)
    {
        return new DecodedResource(ResourceType, Number: -1, Package: 0, Offset: 0, Metadata: new Dictionary<string, object?>(), Payload: data.ToArray());
    }

    public void Encode(DecodedResource resource, Stream destination)
    {
        destination.Write(resource.Payload, 0, resource.Payload.Length);
    }

    public void Validate(DecodedResource resource)
    {
        // No-op for raw codec.
    }
}
