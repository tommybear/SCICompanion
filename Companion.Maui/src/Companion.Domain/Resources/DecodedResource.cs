using System.Collections.Generic;

using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed record DecodedResource(
    ResourcePackage Package,
    byte[] Payload,
    IReadOnlyDictionary<string, object?> Metadata)
{
    public ResourcePackageHeader Header => Package.Header;
    public SCIVersion Version => Package.Version;
    public ResourceType ResourceType => Header.Type;
}
