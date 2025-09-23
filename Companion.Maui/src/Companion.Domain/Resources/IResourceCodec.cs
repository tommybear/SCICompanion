using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public interface IResourceCodec
{
    ResourceType ResourceType { get; }

    DecodedResource Decode(Span<byte> data);

    void Encode(DecodedResource resource, Stream destination);

    void Validate(DecodedResource resource);
}
