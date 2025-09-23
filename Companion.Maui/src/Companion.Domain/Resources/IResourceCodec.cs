using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public interface IResourceCodec
{
    ResourceType ResourceType { get; }

    DecodedResource Decode(ResourcePackage package);

    ResourcePackage Encode(DecodedResource resource);

    void Validate(DecodedResource resource);
}
