namespace Companion.Domain.Assets;

/// <summary>
/// Provides access to curated sample assets used for testing and onboarding.
/// </summary>
public interface ISampleAssetCatalog
{
    IReadOnlyList<SampleAsset> GetAssets();
}
