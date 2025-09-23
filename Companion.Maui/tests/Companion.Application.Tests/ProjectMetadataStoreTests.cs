using Companion.Application.Projects;
using Companion.Application.Tests.Support;
using Companion.Domain.Projects;

namespace Companion.Application.Tests;

public class ProjectMetadataStoreTests
{
    [Fact]
    public async Task SaveAndLoad_RoundTripsMetadata()
    {
        var tempPath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString(), "sample.sciproj");
        Directory.CreateDirectory(Path.GetDirectoryName(tempPath)!);

        var store = new ProjectMetadataStore();
        var metadata = new ProjectMetadata
        {
            Title = "Template SCI0",
            GameFolder = "TemplateGame/SCI0",
            Version = SCIVersion.SCI0,
            Interpreter = new InterpreterProfile(InterpreterProfileKind.DosBox, "Tools/DOSBox/dosbox", "-conf dosbox.conf"),
            Notes = "Test metadata"
        };

        await store.SaveAsync(tempPath, metadata);
        var loaded = await store.LoadAsync(tempPath);

        Assert.Equal(metadata, loaded);
    }
}
