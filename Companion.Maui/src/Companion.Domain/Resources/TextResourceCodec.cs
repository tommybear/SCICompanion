using System.Collections.Generic;
using System.Text;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class TextResourceCodec : IResourceCodec
{
    private const string TextEntriesKey = "TextEntries";
    private const string CompressionMethodKey = "CompressionMethod";

    public ResourceType ResourceType => ResourceType.Text;

    public DecodedResource Decode(ResourcePackage package)
    {
        var texts = ParseStrings(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [TextEntriesKey] = texts,
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

        // TODO: regenerate payload from text entries when editing support arrives.
        return new ResourcePackage(resource.Version, header, resource.Payload);
    }

    public void Validate(DecodedResource resource)
    {
        if (resource.Payload.Length == 0)
        {
            throw new InvalidOperationException("Text payload cannot be empty.");
        }
    }

    private static IReadOnlyList<string> ParseStrings(byte[] payload)
    {
        var list = new List<string>();
        if (payload.Length == 0)
        {
            return list;
        }

        int start = 0;
        for (int i = 0; i < payload.Length; i++)
        {
            if (payload[i] == 0)
            {
                if (i > start)
                {
                    var text = Encoding.ASCII.GetString(payload, start, i - start);
                    list.Add(text);
                }
                start = i + 1;
            }
        }

        if (start < payload.Length)
        {
            var text = Encoding.ASCII.GetString(payload, start, payload.Length - start);
            list.Add(text);
        }

        return list;
    }
}
