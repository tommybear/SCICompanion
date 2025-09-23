namespace Companion.Application.Tests.Support;

public static class RepoPaths
{
    private static readonly Lazy<string> _repoRoot = new(FindRepoRoot);

    public static string RepoRoot => _repoRoot.Value;

    public static string GetTemplateGame(string versionFolder) => Path.Combine(RepoRoot, "TemplateGame", versionFolder);

    private static string FindRepoRoot()
    {
        var current = Directory.GetCurrentDirectory();
        while (current is not null)
        {
            if (Directory.Exists(Path.Combine(current, "TemplateGame")) && File.Exists(Path.Combine(current, "README.md")))
            {
                return current;
            }
            current = Directory.GetParent(current)?.FullName;
        }

        throw new DirectoryNotFoundException("Unable to locate repository root from current working directory.");
    }
}
