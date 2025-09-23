using System.IO;
using Companion.Domain.Projects;

namespace Companion.Domain.Resources;

public sealed record ResourcePackage(
    SCIVersion Version,
    ResourcePackageHeader Header,
    byte[] Body)
{
    public void WriteTo(Stream destination)
    {
        Header.WriteTo(destination, Version);
        destination.Write(Body, 0, Body.Length);
    }
}
