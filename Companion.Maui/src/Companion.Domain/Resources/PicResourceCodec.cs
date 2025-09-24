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
        var parseResult = PicParser.Parse(decodedPayload, package.Version);
        var document = PicRasterizer.Render(parseResult, package.Version);
        var commands = parseResult.Commands;
        var opcodeCounts = PicParser.AggregateOpcodeCounts(commands);

        var metadata = new Dictionary<string, object?>
        {
            [CompressionMethodKey] = package.Header.CompressionMethod,
            [CommandsKey] = commands,
            [OpcodeCountsKey] = opcodeCounts,
            [DecodedPayloadKey] = decodedPayload,
            ["PicDocument"] = document,
            ["PicStateTimeline"] = parseResult.StateTimeline,
            ["PicFinalState"] = parseResult.FinalState
        };
        return new DecodedResource(package, package.Body, metadata);
    }

    public ResourcePackage Encode(DecodedResource resource)
    {
        var header = resource.Header;

        if (resource.Metadata.TryGetValue("PicDocument", out var docValue) && docValue is PicDocument document &&
            resource.Metadata.TryGetValue(DecodedPayloadKey, out var payloadValue) && payloadValue is byte[] originalPayload)
        {
            var encoder = new PicEncoder();
            var encodedPayload = encoder.Encode(document, originalPayload);

            if (header.CompressionMethod == 0)
            {
                header = header with { CompressedLength = encodedPayload.Length };
                if (header.DecompressedLength == 0)
                {
                    header = header with { DecompressedLength = encodedPayload.Length };
                }
                return new ResourcePackage(resource.Version, header, encodedPayload);
            }

            if (header.DecompressedLength == 0)
            {
                header = header with { DecompressedLength = encodedPayload.Length };
            }
            // Compression encoder not yet implemented; reuse original compressed body for now.
            return new ResourcePackage(resource.Version, header, resource.Payload);
        }

        // No PIC metadata available; return original payload.
        header = header with
        {
            CompressedLength = resource.Payload.Length
        };

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
        var method = NormalizeCompressionMethod(package.Version, package.Header.CompressionMethod);
        if (method == 0)
        {
            var copy = new byte[package.Body.Length];
            Array.Copy(package.Body, copy, copy.Length);
            return copy;
        }

        return _compressionRegistry.Decompress(package.Body, method, package.Header.DecompressedLength);
    }

    private static int NormalizeCompressionMethod(SCIVersion version, int method) => method;
}
