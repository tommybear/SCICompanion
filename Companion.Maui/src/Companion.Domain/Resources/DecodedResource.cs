using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed record DecodedResource(
    ResourceType Type,
    int Number,
    int Package,
    uint Offset,
    IReadOnlyDictionary<string, object?> Metadata,
    byte[] Payload
)
{
    public T GetMetadata<T>(string key, T defaultValue = default!)
    {
        if (Metadata.TryGetValue(key, out var value) && value is T typed)
        {
            return typed;
        }
        return defaultValue;
    }
}
