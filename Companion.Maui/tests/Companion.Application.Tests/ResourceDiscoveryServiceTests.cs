using System.Linq;
using Companion.Application.Projects;
using Companion.Application.Tests.Support;
using Companion.Domain.Projects;

namespace Companion.Application.Tests;

public class ResourceDiscoveryServiceTests
{
    private readonly ResourceDiscoveryService _service = new();

    [Fact]
    public void Discover_Sci0Template_ParsesScripts()
    {
        var gameFolder = RepoPaths.GetTemplateGame("SCI0");
        var catalog = _service.Discover(gameFolder);

        Assert.Equal(SCIVersion.SCI0, catalog.Version);
        Assert.Contains(catalog.Resources, r => r.Type == ResourceType.Script && r.Number == 0);
        Assert.NotEmpty(catalog.Resources);
    }

    [Fact]
    public void Discover_Sci11Template_ParsesViews()
    {
        var gameFolder = RepoPaths.GetTemplateGame("SCI1.1");
        var catalog = _service.Discover(gameFolder);

        Assert.Equal(SCIVersion.SCI11, catalog.Version);
        Assert.Contains(catalog.Resources, r => r.Type == ResourceType.View && r.Number == 0);
        Assert.Contains(catalog.Resources, r => r.Type == ResourceType.Pic || r.Type == ResourceType.Palette);
    }
}
