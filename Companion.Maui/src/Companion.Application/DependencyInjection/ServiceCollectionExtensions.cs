using Companion.Application.Assets;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Assets;
using Microsoft.Extensions.DependencyInjection;
using Companion.Domain.Compression;

namespace Companion.Application.DependencyInjection;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddApplicationServices(this IServiceCollection services)
    {
        services.AddSingleton<ISampleAssetCatalog, StaticSampleAssetCatalog>();
        services.AddSingleton<IProjectMetadataStore, ProjectMetadataStore>();
        services.AddSingleton<IResourceDiscoveryService, ResourceDiscoveryService>();
        services.AddSingleton<ICompressionService>(new PassthroughCompressionService(0, 20));
        services.AddSingleton<ICompressionService>(new LzwCompressionService(1));
        services.AddSingleton<ICompressionService>(new Lzw1CompressionService(2));
        services.AddSingleton<ICompressionRegistry, CompressionRegistry>();
        services.AddSingleton<IResourceCodec>(sp => new PicResourceCodec(sp.GetRequiredService<ICompressionRegistry>()));
        services.AddSingleton<IResourceCodec, ViewResourceCodec>();
        services.AddSingleton<IResourceCodec, PaletteResourceCodec>();
        services.AddSingleton<IResourceCodec, TextResourceCodec>();
        services.AddSingleton<IResourceCodec, MessageResourceCodec>();
        services.AddSingleton<IResourceCodec, VocabularyResourceCodec>();
        services.AddSingleton<IResourceCodec, SoundResourceCodec>();
        services.AddSingleton<IResourceCodec>(new RawBinaryCodec(ResourceType.Unknown));
        services.AddSingleton<IResourceCodecRegistry>(sp => new ResourceCodecRegistry(sp.GetServices<IResourceCodec>(), type => new RawBinaryCodec(type)));
        services.AddSingleton<ResourceVolumeReader>();
        services.AddSingleton<IResourceRepository, ResourceRepository>();
        return services;
    }
}
