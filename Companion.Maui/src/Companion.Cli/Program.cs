using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using Companion.Application.DependencyInjection;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Infrastructure.DependencyInjection;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Resources.Pic;
using Companion.Domain.Compression;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using System.Text;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;

var builder = Host.CreateApplicationBuilder(args);
builder.Services
    .AddApplicationServices()
    .AddInfrastructureServices(builder.Configuration)
    .AddSingleton<ProjectInspector>();

using var host = builder.Build();
var inspector = host.Services.GetRequiredService<ProjectInspector>();
await inspector.RunAsync(args);

sealed class ProjectInspector
{
    private readonly IProjectMetadataStore _metadataStore;
    private readonly IResourceDiscoveryService _resourceDiscovery;
    private readonly IResourceRepository _repository;
    private readonly ResourceVolumeReader _volumeReader;
    private readonly IReadOnlyList<ICompressionService> _compressionServices;

    public ProjectInspector(
        IProjectMetadataStore metadataStore,
        IResourceDiscoveryService resourceDiscovery,
        IResourceRepository repository,
        ResourceVolumeReader volumeReader,
        IEnumerable<ICompressionService> compressionServices)
    {
        _metadataStore = metadataStore;
        _resourceDiscovery = resourceDiscovery;
        _repository = repository;
        _volumeReader = volumeReader;
        _compressionServices = compressionServices.ToList();
    }

    public async Task RunAsync(string[] args)
    {
        var options = ParseOptions(args);
        var target = options.TargetPath ?? Directory.GetCurrentDirectory();
        var inspectSelector = options.ResourceSelector;
        target = Path.GetFullPath(target);

        string? projectFile = null;
        string? gameFolder = null;

        if (Directory.Exists(target))
        {
            projectFile = Directory.EnumerateFiles(target, "*.sciproj", SearchOption.TopDirectoryOnly).FirstOrDefault();
            if (projectFile is null)
            {
                gameFolder = target;
            }
        }
        else if (File.Exists(target))
        {
            projectFile = target;
        }
        else
        {
            throw new DirectoryNotFoundException($"Path '{target}' does not exist.");
        }

        ProjectMetadata? metadata = null;
        if (projectFile is not null)
        {
            metadata = await _metadataStore.LoadAsync(projectFile);
            gameFolder = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(projectFile)!, metadata.GameFolder));
        }

        if (gameFolder is null)
        {
            throw new InvalidOperationException("Unable to determine game folder to inspect.");
        }

        var catalog = _resourceDiscovery.Discover(gameFolder);
        var resources = catalog.Resources;

        RenderReport(metadata, gameFolder, catalog.Version, resources);

        if (options.ShowCompressionSummary)
        {
            RenderCompressionSummary(gameFolder, catalog.Version, resources);
        }

        if (!string.IsNullOrWhiteSpace(inspectSelector))
        {
            InspectResource(gameFolder, resources, inspectSelector!);
        }

        if (!string.IsNullOrWhiteSpace(options.DumpSelector))
        {
            DumpResource(gameFolder, catalog.Version, resources, options.DumpSelector!);
        }

        foreach (var export in options.Exports)
        {
            ExportResource(gameFolder, catalog.Version, resources, export);
        }
    }

    private static CliOptions ParseOptions(string[] args)
    {
        string? targetPath = null;
        string? selector = null;
        string? dumpSelector = null;
        var exports = new List<ExportRequest>();
        var showCompression = false;

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            if (string.IsNullOrWhiteSpace(arg))
            {
                continue;
            }

            if (IsCompressionFlag(arg))
            {
                showCompression = true;
                continue;
            }

            if (IsDumpFlag(arg))
            {
                var selectorValue = ReadSelectorValue(arg, args, ref i);
                if (!TryParseSelector(selectorValue, out _, out _))
                {
                    throw new ArgumentException($"Invalid resource selector '{selectorValue}' provided to --dump.");
                }

                dumpSelector = selectorValue;
                continue;
            }

            if (IsExportFlag(arg))
            {
                var exportValue = ReadExportValue(arg, args, ref i);
                exports.Add(ParseExportDefinition(exportValue));
                continue;
            }

            if (selector is null && TryParseSelector(arg, out _, out _))
            {
                selector = arg;
                continue;
            }

            targetPath ??= arg;
        }

        return new CliOptions(targetPath, selector, showCompression, dumpSelector, exports);
    }

    private static bool IsCompressionFlag(string value) =>
        string.Equals(value, "--compression", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(value, "-c", StringComparison.OrdinalIgnoreCase);

    private static bool IsDumpFlag(string value) =>
        string.Equals(value, "--dump", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(value, "-d", StringComparison.OrdinalIgnoreCase) ||
        value.StartsWith("--dump=", StringComparison.OrdinalIgnoreCase);

    private static bool IsExportFlag(string value) =>
        string.Equals(value, "--export", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(value, "-e", StringComparison.OrdinalIgnoreCase) ||
        value.StartsWith("--export=", StringComparison.OrdinalIgnoreCase);

    private static string ReadSelectorValue(string current, string[] args, ref int index)
    {
        var equalsIndex = current.IndexOf('=');
        if (equalsIndex >= 0)
        {
            return current[(equalsIndex + 1)..];
        }

        if (index + 1 >= args.Length)
        {
            throw new ArgumentException("--dump requires a resource selector (e.g. --dump pic:0).");
        }

        index++;
        return args[index];
    }

    private static string ReadExportValue(string current, string[] args, ref int index)
    {
        var equalsIndex = current.IndexOf('=');
        if (equalsIndex >= 0)
        {
            return current[(equalsIndex + 1)..];
        }

        if (index + 1 >= args.Length)
        {
            throw new ArgumentException("--export requires an argument (e.g. --export pic:0=visual:out.pgm).");
        }

        index++;
        return args[index];
    }

    private static void RenderReport(ProjectMetadata? metadata, string gameFolder, SCIVersion version, IReadOnlyList<ResourceDescriptor> resources)
    {
        var writer = new StringBuilder();
        writer.AppendLine($"Game folder: {gameFolder}");
        if (metadata is not null)
        {
            writer.AppendLine($"Project: {metadata.Title}");
            writer.AppendLine($"Interpreter: {metadata.Interpreter.Kind} ({metadata.Interpreter.Executable})");
        }
        writer.AppendLine($"Detected SCI version: {version}");
        writer.AppendLine();

        var grouped = resources
            .GroupBy(r => r.Type)
            .OrderBy(g => g.Key);

        foreach (var group in grouped)
        {
            writer.AppendLine($"[{group.Key}] count={group.Count()}");
            foreach (var descriptor in group.Take(5))
            {
                writer.AppendLine($"  - #{descriptor.Number} pkg={descriptor.Package} offset=0x{descriptor.Offset:X}");
            }
            if (group.Count() > 5)
            {
                writer.AppendLine($"  ... ({group.Count() - 5} more)");
            }
            writer.AppendLine();
        }

        Console.WriteLine(writer.ToString());
    }

    private void RenderCompressionSummary(string gameFolder, SCIVersion version, IReadOnlyList<ResourceDescriptor> resources)
    {
        var stats = new List<CompressionStat>(capacity: resources.Count);
        foreach (var descriptor in resources)
        {
            try
            {
                var package = _volumeReader.Read(gameFolder, descriptor, version);
                stats.Add(new CompressionStat(descriptor, package.Header.CompressionMethod, package.Header.CompressedLength, package.Header.DecompressedLength, null));
            }
            catch (Exception ex)
            {
                stats.Add(new CompressionStat(descriptor, null, null, null, ex));
            }
        }

        var writer = new StringBuilder();
        writer.AppendLine("Compression Summary");
        writer.AppendLine("-------------------");

        var grouped = stats
            .Where(s => s.Method.HasValue)
            .GroupBy(s => s.Method!.Value)
            .OrderBy(g => g.Key);

        foreach (var group in grouped)
        {
            var method = group.Key;
            var name = GetCompressionName(method);
            var count = group.Count();
            var totalCompressed = group.Sum(s => s.CompressedLength ?? 0);
            var totalDecompressed = group.Sum(s => s.DecompressedLength ?? 0);
            var ratio = totalDecompressed == 0 ? "n/a" : (totalCompressed / (double)totalDecompressed).ToString("0.00");
            writer.AppendLine($"  {name} (method {method}): count={count}, compressed={totalCompressed} bytes, decompressed={totalDecompressed} bytes, ratio={ratio}");
        }

        var unsupported = stats
            .Where(s => s.Method.HasValue && !_compressionServices.Any(service => service.SupportsMethod(s.Method.Value)))
            .GroupBy(s => s.Method!.Value)
            .OrderBy(g => g.Key)
            .ToList();

        if (unsupported.Count > 0)
        {
            writer.AppendLine();
            writer.AppendLine("  Unsupported methods:");
            foreach (var group in unsupported)
            {
                writer.AppendLine($"    Method {group.Key} ({group.Count()} resources)");
            }
        }

        var failures = stats.Where(s => s.Error is not null).ToList();
        if (failures.Count > 0)
        {
            writer.AppendLine();
            writer.AppendLine("  Errors:");
            foreach (var failure in failures)
            {
                writer.AppendLine($"    {failure.Descriptor}: {failure.Error!.Message}");
            }
        }

        Console.WriteLine(writer.ToString());
    }

    private string GetCompressionName(int method)
    {
        var service = _compressionServices.FirstOrDefault(s => s.SupportsMethod(method));
        if (service is null)
        {
            return "Unknown";
        }

        var name = service.GetType().Name;
        const string suffix = "CompressionService";
        if (name.EndsWith(suffix, StringComparison.Ordinal))
        {
            name = name.Substring(0, name.Length - suffix.Length);
        }

        return name;
    }

    private void InspectResource(string gameFolder, IReadOnlyList<ResourceDescriptor> resources, string selector)
    {
        if (!TryParseSelector(selector, out var type, out var number))
        {
            Console.WriteLine($"Could not parse selector '{selector}'. Use format type:number (e.g. pic:0).");
            return;
        }

        var descriptor = resources.FirstOrDefault(r => r.Type == type && r.Number == number);
        if (descriptor is null)
        {
            Console.WriteLine($"Resource {type}:{number} not found in catalog.");
            return;
        }

        try
        {
            var decoded = _repository.LoadResource(gameFolder, descriptor);
            Console.WriteLine($"Inspecting {type}:{number}");
            Console.WriteLine($"  Package: {descriptor.Package}");
            Console.WriteLine($"  Offset:  0x{descriptor.Offset:X}");
            Console.WriteLine($"  Payload: {decoded.Payload.Length} bytes");
            Console.WriteLine($"  Compression method: {decoded.Header.CompressionMethod}");

            if (type == ResourceType.Pic && decoded.Metadata.TryGetValue("PicCommands", out var commandValue)
                && commandValue is IReadOnlyCollection<PicCommand> commands)
            {
                Console.WriteLine($"  Commands: {commands.Count}");
            }

            if (type == ResourceType.Palette && decoded.Metadata.TryGetValue("PaletteEntries", out var paletteValue)
                && paletteValue is IReadOnlyList<PaletteEntry> palette)
            {
                Console.WriteLine($"  Palette entries: {palette.Count}");
                foreach (var (r, g, b) in palette.Take(5).Select(p => p.ToTuple()))
                {
                    Console.WriteLine($"    RGB({r},{g},{b})");
                }
                if (palette.Count > 5)
                {
                    Console.WriteLine($"    ... ({palette.Count - 5} more)");
                }
            }

            if (type == ResourceType.Message && decoded.Metadata.TryGetValue("MessageOffsets", out var offsetsValue)
                && offsetsValue is IReadOnlyList<int> offsets)
            {
                Console.WriteLine($"  Message offsets: {offsets.Count}");
                foreach (var offset in offsets.Take(5))
                {
                    Console.WriteLine($"    0x{offset:X}");
                }
                if (offsets.Count > 5)
                {
                    Console.WriteLine($"    ... ({offsets.Count - 5} more)");
                }
            }

            if (type == ResourceType.Sound && decoded.Metadata.TryGetValue("SoundHeader", out var headerValue)
                && headerValue is byte[] soundHeader)
            {
                Console.WriteLine($"  Sound header bytes: {soundHeader.Length}");
                Console.WriteLine($"    {BitConverter.ToString(soundHeader.Take(16).ToArray())}");
            }

            if (type == ResourceType.View && decoded.Metadata.TryGetValue("ViewHeader", out var viewHeaderValue)
                && viewHeaderValue is byte[] viewHeader)
            {
                Console.WriteLine($"  View header bytes: {viewHeader.Length}");
                Console.WriteLine($"    {BitConverter.ToString(viewHeader.Take(16).ToArray())}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to inspect resource {type}:{number}: {ex.Message}");
        }
    }

    private static bool TryParseSelector(string selector, out ResourceType type, out int number)
    {
        type = ResourceType.Unknown;
        number = -1;
        var parts = selector.Split(':', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (parts.Length != 2)
        {
            return false;
        }

        if (!Enum.TryParse(parts[0], ignoreCase: true, out type))
        {
            return false;
        }

        return int.TryParse(parts[1], out number) && number >= 0;
    }

    private void DumpResource(string gameFolder, SCIVersion version, IReadOnlyList<ResourceDescriptor> resources, string selector)
    {
        if (!TryParseSelector(selector, out var type, out var number))
        {
            Console.WriteLine($"Could not parse selector '{selector}'. Use format type:number (e.g. pic:0).");
            return;
        }

        var descriptor = resources.FirstOrDefault(r => r.Type == type && r.Number == number);
        if (descriptor is null)
        {
            Console.WriteLine($"Resource {type}:{number} not found in catalog.");
            return;
        }

        try
        {
            var decoded = _repository.LoadResource(gameFolder, descriptor);
            var package = _volumeReader.Read(gameFolder, descriptor, version);
            Console.WriteLine($"Dumping {type}:{number}");
            Console.WriteLine($"  Package: {descriptor.Package}");
            Console.WriteLine($"  Compression method: {package.Header.CompressionMethod}");
            Console.WriteLine($"  Compressed length: {package.Header.CompressedLength} bytes");
            Console.WriteLine($"  Decompressed length: {decoded.Payload.Length} bytes");
            Console.WriteLine("  Base64 (decompressed payload):");
            foreach (var line in ChunkString(Convert.ToBase64String(decoded.Payload), 76))
            {
                Console.WriteLine($"    {line}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to dump resource {type}:{number}: {ex.Message}");
        }
    }

    private static IEnumerable<string> ChunkString(string value, int length)
    {
        if (string.IsNullOrEmpty(value))
        {
            yield break;
        }

        for (var index = 0; index < value.Length; index += length)
        {
            var chunkLength = Math.Min(length, value.Length - index);
            yield return value.Substring(index, chunkLength);
        }
    }

    private void ExportResource(string gameFolder, SCIVersion version, IReadOnlyList<ResourceDescriptor> resources, ExportRequest request)
    {
        if (!TryParseSelector(request.Selector, out var type, out var number))
        {
            Console.WriteLine($"Could not parse export selector '{request.Selector}'.");
            return;
        }

        var descriptor = resources.FirstOrDefault(r => r.Type == type && r.Number == number);
        if (descriptor is null)
        {
            Console.WriteLine($"Resource {type}:{number} not found in catalog.");
            return;
        }

        try
        {
            var decoded = _repository.LoadResource(gameFolder, descriptor);
            if (type != ResourceType.Pic || !decoded.Metadata.TryGetValue("PicDocument", out var docValue) || docValue is not PicDocument document)
            {
                Console.WriteLine($"Resource {type}:{number} is not a PIC or lacks rendering data.");
                return;
            }

            var planeData = request.Plane switch
            {
                PicPlane.Visual => document.VisualPlane,
                PicPlane.Priority => document.PriorityPlane,
                PicPlane.Control => document.ControlPlane,
                _ => document.VisualPlane
            };

            var directory = Path.GetDirectoryName(request.Path);
            if (!string.IsNullOrEmpty(directory))
            {
                Directory.CreateDirectory(directory);
            }

            var extension = Path.GetExtension(request.Path);
            if (string.Equals(extension, ".png", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(extension, ".bmp", StringComparison.OrdinalIgnoreCase))
            {
                WriteImage(request.Path, document, planeData, request.Plane, version);
            }
            else
            {
                WritePgm(request.Path, document.Width, document.Height, planeData);
            }

            Console.WriteLine($"Exported {type}:{number} {request.Plane} plane to {request.Path}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to export resource {type}:{number}: {ex.Message}");
        }
    }

    private static void WritePgm(string path, int width, int height, byte[] data)
    {
        using var writer = new StreamWriter(path, append: false);
        writer.WriteLine("P2");
        writer.WriteLine($"{width} {height}");
        writer.WriteLine("255");

        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var value = data[y * width + x];
                writer.Write(value);
                writer.Write(' ');
            }
            writer.WriteLine();
        }
    }

    private static void WriteImage(string path, PicDocument document, byte[] data, PicPlane plane, SCIVersion version)
    {
        var image = new Image<Rgba32>(document.Width, document.Height);

        try
        {
            for (var y = 0; y < document.Height; y++)
            {
                for (var x = 0; x < document.Width; x++)
                {
                    var value = data[y * document.Width + x];
                    var color = plane switch
                    {
                        PicPlane.Visual => MapVisualColor(value, version, document.FinalState),
                        PicPlane.Priority => MapPriorityColor(value),
                        PicPlane.Control => MapControlColor(value),
                        _ => new Rgba32(value, value, value)
                    };
                    image[x, y] = color;
                }
            }
            if (plane is PicPlane.Priority or PicPlane.Control)
            {
                using var withLegend = AppendLegend(image, data, plane);
                withLegend.Save(path);
            }
            else
            {
                image.Save(path);
            }
        }
        finally
        {
            image.Dispose();
        }
    }

    private static Rgba32 MapVisualColor(byte value, SCIVersion version, PicStateSnapshot finalState)
    {
        return version <= SCIVersion.SCI0
            ? MapEgaColor(value, finalState.PaletteBanks)
            : MapVgaColor(value, finalState.VgaPalette);
    }

    private static Rgba32 MapPriorityColor(byte value)
    {
        var scaled = (byte)Math.Clamp(value * 17, 0, 255);
        return new Rgba32(scaled, 0, (byte)(255 - scaled));
    }

    private static Rgba32 MapControlColor(byte value)
    {
        var green = value;
        var blue = (byte)(255 - value);
        return new Rgba32(0, green, blue);
    }

    private static Rgba32 MapEgaColor(byte value, byte[][] paletteBanks)
    {
        if (paletteBanks is null || paletteBanks.Length == 0)
        {
            return new Rgba32(value, value, value);
        }

        var paletteIndex = value / 40;
        var colorIndex = value % 40;

        if (paletteIndex >= paletteBanks.Length)
        {
            paletteIndex = paletteBanks.Length - 1;
        }

        var bank = paletteBanks[paletteIndex];
        if (bank is null || bank.Length == 0)
        {
            return new Rgba32(value, value, value);
        }

        if (colorIndex >= bank.Length)
        {
            colorIndex = bank.Length - 1;
        }

        var encoded = bank[colorIndex];

        if (encoded == 0 && colorIndex != 0)
        {
            return GetEgaBaseColor(colorIndex % 16);
        }

        var high = encoded >> 4;
        var low = encoded & 0x0F;
        var primary = GetEgaBaseColor(high);
        var secondary = GetEgaBaseColor(low);

        if (high == low)
        {
            return primary;
        }

        return new Rgba32(
            (byte)((primary.R + secondary.R) / 2),
            (byte)((primary.G + secondary.G) / 2),
            (byte)((primary.B + secondary.B) / 2));
    }

    private static Rgba32 MapVgaColor(byte value, byte[] palette)
    {
        if (palette is null || palette.Length < 3)
        {
            return new Rgba32(value, value, value);
        }

        var index = value * 3;
        if (index + 2 >= palette.Length)
        {
            return new Rgba32(value, value, value);
        }

        var r = NormalizeVgaComponent(palette[index]);
        var g = NormalizeVgaComponent(palette[index + 1]);
        var b = NormalizeVgaComponent(palette[index + 2]);
        return new Rgba32(r, g, b);
    }

    private static byte NormalizeVgaComponent(byte component)
    {
        return component <= 63 ? (byte)(component * 4) : component;
    }

    private static Rgba32 GetEgaBaseColor(int index)
    {
        return index switch
        {
            0 => new Rgba32(0x00, 0x00, 0x00),
            1 => new Rgba32(0x00, 0x00, 0xAA),
            2 => new Rgba32(0x00, 0xAA, 0x00),
            3 => new Rgba32(0x00, 0xAA, 0xAA),
            4 => new Rgba32(0xAA, 0x00, 0x00),
            5 => new Rgba32(0xAA, 0x00, 0xAA),
            6 => new Rgba32(0xAA, 0x55, 0x00),
            7 => new Rgba32(0xAA, 0xAA, 0xAA),
            8 => new Rgba32(0x55, 0x55, 0x55),
            9 => new Rgba32(0x55, 0x55, 0xFF),
            10 => new Rgba32(0x55, 0xFF, 0x55),
            11 => new Rgba32(0x55, 0xFF, 0xFF),
            12 => new Rgba32(0xFF, 0x55, 0x55),
            13 => new Rgba32(0xFF, 0x55, 0xFF),
            14 => new Rgba32(0xFF, 0xFF, 0x55),
            15 => new Rgba32(0xFF, 0xFF, 0xFF),
            _ => new Rgba32(0x00, 0x00, 0x00)
        };
    }

    private static readonly Rgba32 LegendTextColor = new Rgba32(255, 255, 255);

    private static Dictionary<char, string[]> LegendGlyphs { get; } = new()
    {
        ['0'] = new[] { "xxx", "x x", "x x", "x x", "xxx" },
        ['1'] = new[] { " xx", "  x", "  x", "  x", "xxx" },
        ['2'] = new[] { "xxx", "  x", "xxx", "x  ", "xxx" },
        ['3'] = new[] { "xxx", "  x", "xx ", "  x", "xxx" },
        ['4'] = new[] { "x x", "x x", "xxx", "  x", "  x" },
        ['5'] = new[] { "xxx", "x  ", "xxx", "  x", "xxx" },
        ['6'] = new[] { "xxx", "x  ", "xxx", "x x", "xxx" },
        ['7'] = new[] { "xxx", "  x", "  x", "  x", "  x" },
        ['8'] = new[] { "xxx", "x x", "xxx", "x x", "xxx" },
        ['9'] = new[] { "xxx", "x x", "xxx", "  x", "xxx" }
    };

    private const int LegendGlyphWidth = 3;
    private const int LegendGlyphHeight = 5;

    private static Image<Rgba32> AppendLegend(Image<Rgba32> source, byte[] data, PicPlane plane)
    {
        var uniqueValues = data
            .Distinct()
            .OrderBy(v => v)
            .ToArray();

        if (uniqueValues.Length == 0)
        {
            return source.Clone();
        }

        var maxEntries = plane == PicPlane.Priority ? 16 : 20;
        if (uniqueValues.Length > maxEntries)
        {
            uniqueValues = uniqueValues.Take(maxEntries).ToArray();
        }

        const int legendHeight = 28;
        const int legendPadding = 6;
        var barHeight = legendHeight - LegendGlyphHeight - legendPadding;

        var output = new Image<Rgba32>(source.Width, source.Height + legendHeight);

        for (var y = 0; y < source.Height; y++)
        {
            for (var x = 0; x < source.Width; x++)
            {
                output[x, y] = source[x, y];
            }
        }

        var baseY = source.Height;
        for (var y = baseY; y < output.Height; y++)
        {
            for (var x = 0; x < output.Width; x++)
            {
                output[x, y] = new Rgba32(20, 20, 20);
            }
        }

        for (var i = 0; i < uniqueValues.Length; i++)
        {
            var value = uniqueValues[i];
            var startX = (int)Math.Round(i * output.Width / (double)uniqueValues.Length);
            var endX = (int)Math.Round((i + 1) * output.Width / (double)uniqueValues.Length);
            if (endX <= startX)
            {
                endX = startX + 1;
            }

            var color = plane switch
            {
                PicPlane.Priority => MapPriorityColor(value),
                PicPlane.Control => MapControlColor(value),
                _ => new Rgba32(value, value, value)
            };

            for (var y = baseY; y < baseY + barHeight; y++)
            {
                for (var x = startX; x < endX && x < output.Width; x++)
                {
                    output[x, y] = color;
                }
            }

            var label = value.ToString(CultureInfo.InvariantCulture);
            var labelWidth = label.Length * (LegendGlyphWidth + 1) - 1;
            var labelStartX = startX + ((endX - startX) - labelWidth) / 2;
            if (labelStartX < 0)
            {
                labelStartX = startX;
            }

            DrawLegendText(output, label, labelStartX, baseY + barHeight + 1, LegendTextColor);
        }

        return output;
    }

    private static void DrawLegendText(Image<Rgba32> image, string text, int startX, int startY, Rgba32 color)
    {
        if (string.IsNullOrEmpty(text))
        {
            return;
        }

        var x = startX;
        foreach (var ch in text)
        {
            if (!LegendGlyphs.TryGetValue(ch, out var glyph))
            {
                x += LegendGlyphWidth + 1;
                continue;
            }

            for (var gy = 0; gy < glyph.Length; gy++)
            {
                var row = glyph[gy];
                for (var gx = 0; gx < row.Length; gx++)
                {
                    if (row[gx] == 'x')
                    {
                        var pixelX = x + gx;
                        var pixelY = startY + gy;
                        if (pixelX >= 0 && pixelX < image.Width && pixelY >= 0 && pixelY < image.Height)
                        {
                            image[pixelX, pixelY] = color;
                        }
                    }
                }
            }

            x += LegendGlyphWidth + 1;
        }
    }

    private static ExportRequest ParseExportDefinition(string value)
    {
        var selectorSplit = value.Split('=', 2, StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        if (selectorSplit.Length != 2)
        {
            throw new ArgumentException($"Invalid --export value '{value}'. Expected format selector=plane:path");
        }

        var selector = selectorSplit[0];
        var planeSplit = selectorSplit[1].Split(':', 2, StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        if (planeSplit.Length != 2)
        {
            throw new ArgumentException($"Invalid --export value '{value}'. Expected format selector=plane:path");
        }

        if (!Enum.TryParse<PicPlane>(planeSplit[0], true, out var plane))
        {
            throw new ArgumentException($"Unknown plane '{planeSplit[0]}' in --export.");
        }

        var outputPath = Path.GetFullPath(planeSplit[1]);
        return new ExportRequest(selector, plane, outputPath);
    }

    private sealed record CliOptions(string? TargetPath, string? ResourceSelector, bool ShowCompressionSummary, string? DumpSelector, IReadOnlyList<ExportRequest> Exports);

    private sealed record CompressionStat(
        ResourceDescriptor Descriptor,
        int? Method,
        int? CompressedLength,
        int? DecompressedLength,
        Exception? Error);

    private sealed record ExportRequest(string Selector, PicPlane Plane, string Path);

    private enum PicPlane
    {
        Visual,
        Priority,
        Control
    }
}
