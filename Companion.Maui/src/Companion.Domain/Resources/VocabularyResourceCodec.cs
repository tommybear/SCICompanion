using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class VocabularyResourceCodec : IResourceCodec
{
    private const string VocabularyEntriesKey = "VocabularyEntries";

    public ResourceType ResourceType => ResourceType.Vocab;

    public DecodedResource Decode(ResourcePackage package)
    {
        var entries = ParseVocabulary(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [VocabularyEntriesKey] = entries,
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
            throw new InvalidOperationException("Vocabulary payload cannot be empty.");
        }
    }

    private static IReadOnlyList<string> ParseVocabulary(byte[] payload)
    {
        var list = new List<string>();
        int start = 0;
        for (int i = 0; i < payload.Length; i++)
        {
            if (payload[i] == 0)
            {
                if (i > start)
                {
                    var word = System.Text.Encoding.ASCII.GetString(payload, start, i - start);
                    list.Add(word);
                }
                start = i + 1;
            }
        }
        if (start < payload.Length)
        {
            var word = System.Text.Encoding.ASCII.GetString(payload, start, payload.Length - start);
            list.Add(word);
        }
        return list;
    }
}
