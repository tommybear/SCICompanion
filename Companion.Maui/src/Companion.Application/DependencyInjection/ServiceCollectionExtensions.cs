using Companion.Application.Assets;
using Companion.Application.Projects;
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
        return services;
    }
}
