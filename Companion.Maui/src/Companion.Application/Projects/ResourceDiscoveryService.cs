using System.Buffers.Binary;
using Companion.Domain.Projects;

namespace Companion.Application.Projects;

public interface IResourceDiscoveryService
{
    ResourceCatalog Discover(string gameFolder);
}

public sealed class ResourceDiscoveryService : IResourceDiscoveryService
{
    public ResourceCatalog Discover(string gameFolder)
    {
        var mapPath = Path.Combine(gameFolder, "resource.map");
        if (!File.Exists(mapPath))
        {
            throw new FileNotFoundException("resource.map not found", mapPath);
        }

        var data = File.ReadAllBytes(mapPath);
        if (data.Length == 0)
        {
            throw new InvalidDataException("resource.map is empty");
        }

        var format = DetectFormat(data);
        var resources = format switch
        {
            ResourceMapFormat.Sci0 => ParseSci0(data),
            ResourceMapFormat.Sci1 => ParseSci1(data),
            ResourceMapFormat.Sci11 => ParseSci11(data),
            _ => throw new InvalidOperationException("Unsupported resource map format")
        };

        return new ResourceCatalog(MapFormatToVersion(format), resources);
    }

    private static SCIVersion MapFormatToVersion(ResourceMapFormat format) => format switch
    {
        ResourceMapFormat.Sci0 => SCIVersion.SCI0,
        ResourceMapFormat.Sci1 => SCIVersion.SCI1,
        ResourceMapFormat.Sci11 => SCIVersion.SCI11,
        _ => SCIVersion.Unknown
    };

    private static ResourceType ToResourceType(int id)
    {
        return Enum.IsDefined(typeof(ResourceType), id)
            ? (ResourceType)id
            : ResourceType.Unknown;
    }

    private static IReadOnlyList<ResourceDescriptor> ParseSci0(byte[] data)
    {
        var descriptors = new List<ResourceDescriptor>();
        var offsetBits = 26;
        var offsetMask = (1u << offsetBits) - 1u;
        var packageShift = offsetBits;
        var packageMask = (1u << (32 - offsetBits)) - 1u;

        for (var i = 0; i + 6 <= data.Length; i += 6)
        {
            var typeAndNumber = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(i, 2));
            var body = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(i + 2, 4));

            if (typeAndNumber == 0xFFFF && body == 0xFFFFFFFF)
            {
                break;
            }

            var typeId = typeAndNumber >> 11;
            var number = typeAndNumber & 0x7FF;
            var offset = body & offsetMask;
            var package = (int)((body >> packageShift) & packageMask);

            descriptors.Add(new ResourceDescriptor(ToResourceType(typeId), number, package, offset, null));
        }

        return descriptors;
    }

    private static IReadOnlyList<ResourceDescriptor> ParseSci1(byte[] data)
    {
        var descriptors = new List<ResourceDescriptor>();
        var preEntries = ParsePreEntries(data);
        var entrySize = DetermineEntrySize(preEntries);
        if (entrySize != 6)
        {
            throw new InvalidDataException("Unexpected SCI1 resource entry size");
        }

        foreach (var (typeByte, start, end) in IterateBlocks(preEntries))
        {
            var resourceType = ToResourceType(typeByte & 0x1F);
            for (var pos = start; pos + entrySize <= end; pos += entrySize)
            {
                var number = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(pos, 2));
                var body = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(pos + 2, 4));
                var offset = body & ((1u << 28) - 1u);
                var package = (int)((body >> 28) & 0xFu);
                descriptors.Add(new ResourceDescriptor(resourceType, number, package, offset, null));
            }
        }

        return descriptors;
    }

    private static IReadOnlyList<ResourceDescriptor> ParseSci11(byte[] data)
    {
        var descriptors = new List<ResourceDescriptor>();
        var preEntries = ParsePreEntries(data);
        var entrySize = DetermineEntrySize(preEntries);
        if (entrySize != 5)
        {
            throw new InvalidDataException("Unexpected SCI1.1 resource entry size");
        }

        foreach (var (typeByte, start, end) in IterateBlocks(preEntries))
        {
            var resourceType = ToResourceType(typeByte & 0x1F);
            for (var pos = start; pos + entrySize <= end; pos += entrySize)
            {
                var span = data.AsSpan(pos, entrySize);
                var number = BinaryPrimitives.ReadUInt16LittleEndian(span[..2]);
                var offsetLow = BinaryPrimitives.ReadUInt16LittleEndian(span.Slice(2, 2));
                var offsetHigh = span[4];
                var offset = ((uint)offsetHigh << 16 | offsetLow) << 1;
                descriptors.Add(new ResourceDescriptor(resourceType, number, 0, offset, null));
            }
        }

        return descriptors;
    }

    private static ResourceMapFormat DetectFormat(byte[] data)
    {
        if (data[0] != 0x80)
        {
            // SCI0 style maps are multiples of six bytes.
            if (data.Length % 6 != 0)
            {
                throw new InvalidDataException("resource.map size is not compatible with SCI0 layout");
            }
            return ResourceMapFormat.Sci0;
        }

        var preEntries = ParsePreEntries(data);
        var entrySize = DetermineEntrySize(preEntries);
        return entrySize switch
        {
            6 => ResourceMapFormat.Sci1,
            5 => ResourceMapFormat.Sci11,
            _ => throw new InvalidDataException("Unable to determine resource map entry size")
        };
    }

    private static List<(byte Type, int Offset)> ParsePreEntries(byte[] data)
    {
        var entries = new List<(byte Type, int Offset)>();
        var cursor = 0;
        while (cursor + 3 <= data.Length)
        {
            var typeByte = data[cursor];
            var offset = data[cursor + 1] | (data[cursor + 2] << 8);
            cursor += 3;
            entries.Add((Type: typeByte, Offset: offset));
            if (typeByte == 0xFF)
            {
                break;
            }
        }

        if (entries.Count == 0 || entries[^1].Type != 0xFF)
        {
            throw new InvalidDataException("resource.map missing terminator entry");
        }

        return entries;
    }

    private static int DetermineEntrySize(List<(byte Type, int Offset)> preEntries)
    {
        var gcd = 0;
        for (var i = 0; i < preEntries.Count - 1; i++)
        {
            var diff = preEntries[i + 1].Offset - preEntries[i].Offset;
            if (diff <= 0)
            {
                continue;
            }
            gcd = gcd == 0 ? diff : Gcd(gcd, diff);
        }
        return gcd;
    }

    private static IEnumerable<(byte typeByte, int start, int end)> IterateBlocks(List<(byte Type, int Offset)> preEntries)
    {
        for (var i = 0; i < preEntries.Count - 1; i++)
        {
            var (typeByte, start) = preEntries[i];
            var end = preEntries[i + 1].Offset;
            if (typeByte == 0xFF || end <= start)
            {
                continue;
            }
            yield return (typeByte, start, end);
        }
    }

    private static int Gcd(int a, int b)
    {
        while (b != 0)
        {
            (a, b) = (b, a % b);
        }
        return Math.Abs(a);
    }

    private enum ResourceMapFormat
    {
        Sci0,
        Sci1,
        Sci11
    }
}
