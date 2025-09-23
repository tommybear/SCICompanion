namespace Companion.Domain.Projects;

public sealed record ResourceDescriptor(
    ResourceType Type,
    int Number,
    int Package,
    uint Offset,
    uint? Length
)
{
    public override string ToString() => $"{Type} {Number} (pkg {Package}, offset 0x{Offset:X})";
}

public sealed record ResourceCatalog(
    SCIVersion Version,
    IReadOnlyList<ResourceDescriptor> Resources
);
