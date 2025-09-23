using System.Collections.Generic;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed class PaletteResourceCodec : IResourceCodec
{
    private const string PaletteEntriesKey = "PaletteEntries";
    private const string CompressionMethodKey = "CompressionMethod";

    public ResourceType ResourceType => ResourceType.Palette;

    public DecodedResource Decode(ResourcePackage package)
    {
        var entries = ParseEntries(package.Body);
        var metadata = new Dictionary<string, object?>
        {
            [PaletteEntriesKey] = entries,
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

        // Future work: regenerate payload from palette entries
        return new ResourcePackage(resource.Version, header, resource.Payload);
    }

    public void Validate(DecodedResource resource)
    {
        if (resource.Payload.Length == 0)
        {
            throw new InvalidOperationException("Palette payload cannot be empty.");
        }
    }

    private static IReadOnlyList<PaletteEntry> ParseEntries(byte[] payload)
    {
        var list = new List<PaletteEntry>();
        for (int i = 0; i < payload.Length - 3;)
        {
            if (payload[i] == 0x03 && i + 3 < payload.Length)
            {
                list.Add(new PaletteEntry(payload[i + 1], payload[i + 2], payload[i + 3]));
                i += 4;
            }
            else
            {
                i++;
            }
        }
        return list;
    }
}
