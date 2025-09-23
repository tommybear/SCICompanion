# M0 Scope & Licensing Notes

## Clean-Room Rewrite Scope
- Replace the legacy MFC/Win32 IDE with a cross-platform .NET 8 MAUI desktop application.
- Preserve support for SCI0 through SCI1.1 resource authoring, compilation, and tooling.
- Limit initial footprint to desktop-class operating systems (Windows 11, macOS 14+, Ubuntu 22.04 LTS or equivalent).
- Retain external interpreter launch model (DOSBox, ScummVM, custom) without in-app emulation.
- Deliver new managed plugin architecture; legacy native plugins remain unsupported.

## Licensing Considerations
- Original SCI Companion sources are GPLv2+. A clean-room rewrite must avoid copying GPL-covered implementation details to support alternative licensing.
- Adopt a dual-license strategy pending stakeholder approval: permissive base (e.g., MIT) for original managed code, with optional GPL compatibility shims for interoperability.
- Enforce information barriers: design team may inspect behavior and formats, but new implementation code must be independently authored.
- Track external dependencies (SkiaSharp, Serilog, etc.) with SPDX identifiers and ensure license compatibility before integration.

## Platform Targets & Toolchain
- Target framework: `net8.0` with `windows`, `maccatalyst`, and `linux` (Gtk) workloads.
- Required SDK workloads: `maui`, `maccatalyst`, `android` (build dependency), `ios` (for tooling), with desktop runtime deployments prioritized.
- CI agents: Windows (VS 2022 build tools), macOS (Xcode + .NET workloads), Linux (dotnet + Skia system deps).

## Sample Asset Catalog
The existing repository already contains Sierra SCI samples that will seed fixture-based testing:

| Category | Path | Notes |
| --- | --- | --- |
| SCI0 Template Game | `TemplateGame/SCI0` | Canonical project structure with scripts, resources, and interpreter files.
| SCI1.1 Template Game | `TemplateGame/SCI1.1` | Later engine assets showcasing palette cycling and advanced views.
| Sample Resources | `Samples/SCI0`, `Samples/SCI1.1` | Curated rooms, scripts, and binary resources for regression tests.
| Audio/Lip-Sync Fixtures | `Samples/PhonemeSentence.wav`, `Samples/PhonemeSentence.txt` | Baseline for phoneme timing verification.
| Phoneme Maps | `Samples/default_phoneme_map.ini`, `Samples/sample_phoneme_map.ini` | Template mapping files for lip-sync pipeline.

## Next Steps
- Confirm licensing strategy with legal counsel.
- Mirror sample assets into the new test fixture directory once project layout is in place.
- Document scope summary in project README after MAUI solution scaffold is committed.
