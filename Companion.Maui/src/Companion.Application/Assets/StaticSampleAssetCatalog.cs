using Companion.Domain.Assets;

namespace Companion.Application.Assets;

internal sealed class StaticSampleAssetCatalog : ISampleAssetCatalog
{
    private static readonly IReadOnlyList<SampleAsset> _assets = new List<SampleAsset>
    {
        new("Template (SCI0)", "TemplateGame/SCI0", "Baseline project structure for classic interpreter"),
        new("Template (SCI1.1)", "TemplateGame/SCI1.1", "Template showcasing palette cycling and advanced views"),
        new("Sample Resources (SCI0)", "Samples/SCI0", "Curated collection for regression testing"),
        new("Sample Resources (SCI1.1)", "Samples/SCI1.1", "Reference assets for later interpreter"),
        new("Audio Fixture", "Samples/PhonemeSentence.wav", "Waveform used for lip-sync verification"),
        new("Phoneme Transcript", "Samples/PhonemeSentence.txt", "Benchmark transcript for phoneme mapping"),
        new("Default Phoneme Map", "Samples/default_phoneme_map.ini", "Base mapping for lip-sync pipeline"),
        new("Sample Phoneme Map", "Samples/sample_phoneme_map.ini", "Alternative mapping for comparison")
    };

    public IReadOnlyList<SampleAsset> GetAssets() => _assets;
}
