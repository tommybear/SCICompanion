namespace Companion.Domain.Assets;

/// <summary>
/// Describes a sample SCI asset collection that can seed fixture-based tests.
/// </summary>
/// <param name="Category">Human readable grouping (e.g., SCI0 Template Game).</param>
/// <param name="RelativePath">Path relative to the repository root.</param>
/// <param name="Description">Short guidance on how the asset should be used.</param>
public sealed record SampleAsset(string Category, string RelativePath, string Description);
