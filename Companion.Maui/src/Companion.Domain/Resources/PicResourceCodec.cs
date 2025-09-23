using System;
using System.Collections.Generic;
using Companion.Domain.Compression;
using Companion.Domain.Projects;
using Companion.Domain.Resources.Pic;

namespace Companion.Domain.Resources;

public sealed class PicResourceCodec : IResourceCodec
{
    private const string CompressionMethodKey = "CompressionMethod";
    private const string CommandsKey = "PicCommands";
    private const string OpcodeCountsKey = "PicOpcodeCounts";
    private const string DecodedPayloadKey = "PicDecodedPayload";

    private readonly ICompressionRegistry _compressionRegistry;

    public PicResourceCodec(ICompressionRegistry compressionRegistry)
    {
        _compressionRegistry = compressionRegistry;
    }

    public ResourceType ResourceType => ResourceType.Pic;

    public DecodedResource Decode(ResourcePackage package)
    {
        var decodedPayload = DecodePayload(package);
        var commands = PicParser.Parse(decodedPayload);
        var opcodeCounts = PicParser.AggregateOpcodeCounts(commands);

        var metadata = new Dictionary<string, object?>
        {
            [CompressionMethodKey] = package.Header.CompressionMethod,
            [CommandsKey] = commands,
            [OpcodeCountsKey] = opcodeCounts,
            [DecodedPayloadKey] = decodedPayload
        };
        return new DecodedResource(package, package.Body, metadata);
    }

    public ResourcePackage Encode(DecodedResource resource)
    {
        var header = resource.Header with
        {
            CompressedLength = resource.Payload.Length
        };

        // Future work: re-encode commands from metadata if available.
        return new ResourcePackage(resource.Version, header, resource.Payload);
    }

    public void Validate(DecodedResource resource)
    {
        if (resource.Payload.Length == 0)
        {
            throw new InvalidOperationException("PIC resource payload cannot be empty.");
        }
    }

    private byte[] DecodePayload(ResourcePackage package)
    {
        var method = package.Header.CompressionMethod;
        if (method == 0)
        {
            var copy = new byte[package.Body.Length];
            Array.Copy(package.Body, copy, copy.Length);
            return copy;
        }

        return _compressionRegistry.Decompress(package.Body, method, package.Header.DecompressedLength);
    }
}
