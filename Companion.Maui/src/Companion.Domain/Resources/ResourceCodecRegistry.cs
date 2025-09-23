using System.Collections.Generic;
using System.Linq;

using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public interface IResourceCodecRegistry
{
    IResourceCodec GetCodec(ResourceType type);
}

public sealed class ResourceCodecRegistry : IResourceCodecRegistry
{
    private readonly IReadOnlyDictionary<ResourceType, IResourceCodec> _codecs;

    public ResourceCodecRegistry(IEnumerable<IResourceCodec> codecs)
    {
        _codecs = codecs.ToDictionary(codec => codec.ResourceType);
    }

    public IResourceCodec GetCodec(ResourceType type)
    {
        if (_codecs.TryGetValue(type, out var codec))
        {
            return codec;
        }

        throw new KeyNotFoundException($"No codec registered for resource type '{type}'.");
    }
}
