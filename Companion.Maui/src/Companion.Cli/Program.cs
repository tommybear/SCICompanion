using System.Linq;
using Companion.Application.DependencyInjection;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Infrastructure.DependencyInjection;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Resources.Pic;
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

    public ProjectInspector(
        IProjectMetadataStore metadataStore,
        IResourceDiscoveryService resourceDiscovery,
        IResourceRepository repository)
    {
        _metadataStore = metadataStore;
        _resourceDiscovery = resourceDiscovery;
        _repository = repository;
    }

    public async Task RunAsync(string[] args)
    {
        var target = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        var inspectSelector = args.Length > 1 ? args[1] : null;
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

        if (!string.IsNullOrWhiteSpace(inspectSelector))
        {
            InspectResource(gameFolder, resources, inspectSelector!);
        }
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
}
