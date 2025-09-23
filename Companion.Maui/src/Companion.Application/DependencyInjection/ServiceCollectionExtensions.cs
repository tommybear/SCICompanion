using Companion.Application.Assets;
using Companion.Application.Projects;
using Companion.Application.Resources;
using Companion.Domain.Projects;
using Companion.Domain.Resources;
using Companion.Domain.Assets;
using Microsoft.Extensions.DependencyInjection;

namespace Companion.Application.DependencyInjection;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddApplicationServices(this IServiceCollection services)
    {
        services.AddSingleton<ISampleAssetCatalog, StaticSampleAssetCatalog>();
        services.AddSingleton<IProjectMetadataStore, ProjectMetadataStore>();
        services.AddSingleton<IResourceDiscoveryService, ResourceDiscoveryService>();
        services.AddSingleton<IResourceCodec, PicResourceCodec>();
        services.AddSingleton<IResourceCodec>(new RawBinaryCodec(ResourceType.Unknown));
        services.AddSingleton<IResourceCodecRegistry>(sp => new ResourceCodecRegistry(sp.GetServices<IResourceCodec>()));
        services.AddSingleton<ResourceVolumeReader>();
        return services;
    }
}
