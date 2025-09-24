using System.Diagnostics;
using Companion.Application.Tests.Support;
using Xunit;

namespace Companion.Application.Tests;

public class PicBaselineComparisonTests
{
    private static readonly string CliProjectPath = Path.Combine(RepoPaths.RepoRoot, "Companion.Maui", "src", "Companion.Cli", "Companion.Cli.csproj");

    [Theory]
    [InlineData("SCI0")]
    [InlineData("SCI1.1")]
    public void RendererMatchesCapturedBaselines(string versionFolder)
    {
        var baselineDir = Path.Combine(RepoPaths.RepoRoot, "Companion.Maui", "tests", "Baselines", "Pic", versionFolder);
        Assert.True(Directory.Exists(baselineDir), $"Baseline directory '{baselineDir}' not found.");

        var templateArg = Path.Combine("TemplateGame", versionFolder);

        var psi = new ProcessStartInfo("dotnet")
        {
            WorkingDirectory = RepoPaths.RepoRoot,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        psi.ArgumentList.Add("run");
        psi.ArgumentList.Add("--project");
        psi.ArgumentList.Add(CliProjectPath);
        psi.ArgumentList.Add("--");
        psi.ArgumentList.Add(templateArg);
        psi.ArgumentList.Add("--compare-baseline");
        psi.ArgumentList.Add(baselineDir);

        using var process = Process.Start(psi);
        Assert.NotNull(process);
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            throw new Xunit.Sdk.XunitException($"CLI baseline comparison failed (exit {process.ExitCode}).\nSTDOUT:\n{output}\nSTDERR:\n{error}");
        }

        var lines = output.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var comparisonLines = lines.Where(l => l.StartsWith("Baseline compare", StringComparison.OrdinalIgnoreCase)).ToList();
        Assert.NotEmpty(comparisonLines);

        foreach (var line in comparisonLines)
        {
            var mismatchedIndex = line.LastIndexOf(':');
            Assert.True(mismatchedIndex >= 0, $"Unexpected output line: {line}");

            var metricsPart = line[(mismatchedIndex + 1)..].Trim();
            var slashIndex = metricsPart.IndexOf('/');
            Assert.True(slashIndex > 0, $"Could not parse diff metrics from '{line}'");

            var mismatchedText = metricsPart[..slashIndex].Trim();
            Assert.True(int.TryParse(mismatchedText, out var mismatched), $"Invalid mismatch value in '{line}'");
            Assert.Equal(0, mismatched);
        }
    }
}
