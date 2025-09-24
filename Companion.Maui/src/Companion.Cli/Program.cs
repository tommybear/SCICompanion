using System.Collections.Generic;
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
    }

    private static CliOptions ParseOptions(string[] args)
    {
        string? targetPath = null;
        string? selector = null;
        var showCompression = false;

        foreach (var arg in args)
        {
            if (string.IsNullOrWhiteSpace(arg))
            {
                continue;
            }

            if (IsCompressionFlag(arg))
            {
                showCompression = true;
                continue;
            }

            if (selector is null && TryParseSelector(arg, out _, out _))
            {
                selector = arg;
                continue;
            }

            targetPath ??= arg;
        }

        return new CliOptions(targetPath, selector, showCompression);
    }

    private static bool IsCompressionFlag(string value) =>
        string.Equals(value, "--compression", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(value, "-c", StringComparison.OrdinalIgnoreCase);

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

    private sealed record CliOptions(string? TargetPath, string? ResourceSelector, bool ShowCompressionSummary);

    private sealed record CompressionStat(
        ResourceDescriptor Descriptor,
        int? Method,
        int? CompressedLength,
        int? DecompressedLength,
        Exception? Error);
}
