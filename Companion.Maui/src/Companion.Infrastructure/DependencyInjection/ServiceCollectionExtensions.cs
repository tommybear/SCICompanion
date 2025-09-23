using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;

namespace Companion.Infrastructure.DependencyInjection;

public static class ServiceCollectionExtensions
{
    public static IServiceCollection AddInfrastructureServices(this IServiceCollection services, IConfiguration configuration)
    {
        // Placeholder for future infrastructure registrations (storage, logging sinks, etc.)
        return services;
    }
}
