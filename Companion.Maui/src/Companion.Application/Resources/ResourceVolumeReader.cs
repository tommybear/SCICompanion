using System.Buffers.Binary;
using Companion.Domain.Projects;
using Companion.Domain.Resources;

using System.IO;

namespace Companion.Application.Resources;

public sealed class ResourceVolumeReader
{
    public ResourcePackage Read(string gameFolder, ResourceDescriptor descriptor, SCIVersion version)
    {
        var packagePath = ResolvePackagePath(gameFolder, descriptor.Package);
        using var stream = File.OpenRead(packagePath);
        stream.Seek(descriptor.Offset, SeekOrigin.Begin);
        using var reader = new BinaryReader(stream, System.Text.Encoding.UTF8, leaveOpen: true);

        return version switch
        {
            SCIVersion.SCI0 => ReadSci0(reader, descriptor, version),
            SCIVersion.SCI1 or SCIVersion.SCI11 => ReadSci1(reader, descriptor, version),
            _ => throw new NotSupportedException($"Unsupported SCI version '{version}' for resource loading.")
        };
    }

    private static ResourcePackage ReadSci0(BinaryReader reader, ResourceDescriptor descriptor, SCIVersion version)
    {
        var typeAndNumber = reader.ReadUInt16();
        var type = (ResourceType)(typeAndNumber >> 11);
        var number = typeAndNumber & 0x7FF;
        var compressed = reader.ReadUInt16();
        var decompressed = reader.ReadUInt16();
        var method = reader.ReadUInt16();

        var body = reader.ReadBytes(compressed);

        var header = new ResourcePackageHeader(type, number, descriptor.Package, compressed, decompressed, method);
        return new ResourcePackage(version, header, body);
    }

    private static ResourcePackage ReadSci1(BinaryReader reader, ResourceDescriptor descriptor, SCIVersion version)
    {
        var typeByte = reader.ReadByte();
        var type = (ResourceType)(typeByte & 0x1F);
        var number = reader.ReadUInt16();
        var compressed = reader.ReadUInt16();
        var decompressed = reader.ReadUInt16();
        var method = reader.ReadUInt16();

        var body = reader.ReadBytes(compressed);

        var header = new ResourcePackageHeader(type, number, descriptor.Package, compressed, decompressed, method);
        return new ResourcePackage(version, header, body);
    }

    private static string ResolvePackagePath(string gameFolder, int package)
    {
        var path = Path.Combine(gameFolder, $"resource.{package:000}");
        if (!File.Exists(path))
        {
            throw new FileNotFoundException($"Resource volume for package {package} was not found.", path);
        }
        return path;
    }
}
