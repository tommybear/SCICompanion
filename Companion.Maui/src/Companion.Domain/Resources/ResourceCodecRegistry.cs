using System.Collections.Concurrent;
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
    private readonly ConcurrentDictionary<ResourceType, IResourceCodec> _codecs;
    private readonly Func<ResourceType, IResourceCodec>? _fallbackFactory;

    public ResourceCodecRegistry(IEnumerable<IResourceCodec> codecs, Func<ResourceType, IResourceCodec>? fallbackFactory = null)
    {
        _codecs = new ConcurrentDictionary<ResourceType, IResourceCodec>(codecs.ToDictionary(codec => codec.ResourceType));
        _fallbackFactory = fallbackFactory;
    }

    public IResourceCodec GetCodec(ResourceType type)
    {
        if (_codecs.TryGetValue(type, out var codec))
        {
            return codec;
        }

        if (_fallbackFactory is not null)
        {
            codec = _fallbackFactory(type);
            return _codecs.GetOrAdd(type, codec);
        }

        throw new KeyNotFoundException($"No codec registered for resource type '{type}'.");
    }
}
