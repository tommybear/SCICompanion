# Development Environment Setup (Milestone 0)

1. **Install .NET 8 SDK** with MAUI workloads:
   ```bash
   dotnet workload install maui maccatalyst android ios
   ```
   > Linux users: follow the MAUI Linux preview instructions and install required Gtk/Skia dependencies.

2. **Restore workloads** (network access required):
   ```bash
   dotnet restore Companion.Maui/Companion.Maui.sln
   ```

3. **Run the bootstrap app** to verify the shell:
   ```bash
   dotnet build Companion.Maui/Companion.Maui.sln
   dotnet run --project Companion.Maui/src/Companion.App/Companion.App.csproj -f net8.0-maccatalyst
   ```

4. **Inspect resources via CLI** (optional) to validate map parsing:
   ```bash
   dotnet run --project Companion.Maui/src/Companion.Cli/Companion.Cli.csproj -- TemplateGame/SCI0
   # Inspect a specific resource (type:number)
   dotnet run --project Companion.Maui/src/Companion.Cli/Companion.Cli.csproj -- TemplateGame/SCI0 pic:0
   ```

5. **Sample assets** referenced by the app reside under the repository root (see `docs/M0-Scope.md`). Ensure they remain intact for characterization tests.

## Known Issues
- CI and local restores require internet access to nuget.org. In restricted environments, configure an offline NuGet feed before building.
- MAUI Linux support remains preview-only; expect limitations until the upstream project stabilizes.
