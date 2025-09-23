# SCICompanion Project Plan

## Overview

SCICompanion is a comprehensive IDE and toolkit for authoring, editing, and managing resources for Sierra Creative Interpreter (SCI) games. SCI was the engine powering many classic adventure games from the late 1980s and early 1990s, such as King's Quest, Space Quest, and Leisure Suit Larry series. These games featured point-and-click interfaces, scripted interactions, and rich multimedia resources including vector-based backgrounds, animated sprites, text dialogs, audio, and more.

The original SCICompanion was a Windows-only application built with MFC/C++, providing tools for:
- Editing PIC (Picture) resources: Vector-based backgrounds with visual, priority, and control layers
- Creating and animating VIEW resources: Sprites with loops and cels
- Managing palettes, text tables, vocabulary, and audio
- Compiling and decompiling SCI scripts
- Running games through external interpreters like DOSBox or ScummVM

This project represents a clean-room rewrite of SCICompanion as a cross-platform .NET MAUI application, targeting desktop platforms (Windows, macOS, Linux) while maintaining full feature parity for SCI0 through SCI1.1 game development.

## Project Goals

### Primary Objectives
- **Cross-Platform Compatibility**: Deliver a native desktop experience on Windows 11+, macOS 14+, and Ubuntu 22.04+ using .NET 8 MAUI
- **Modern Architecture**: Implement a layered, testable codebase with clean separation of concerns (Domain, Application, Infrastructure, UI)
- **Feature Parity**: Preserve all core functionality from the legacy tool, including advanced features like lip-sync generation and plugin support
- **Test-Driven Development**: Build with comprehensive automated tests (unit, integration, snapshot, property-based) to ensure correctness and prevent regressions
- **Extensibility**: Provide a managed plugin system for third-party extensions and future SCI version support

### Non-Goals
- Mobile platform support (iOS, Android)
- In-app game emulation (relies on external interpreters)
- SCI2+ resource formats (scoped to SCI0-SCI1.1)
- Binary compatibility with legacy native plugins

## Architecture Overview

The rewrite follows a clean architecture pattern with four main layers:

### Companion.Domain
Core business logic and data models, platform-agnostic and fully testable:
- Resource models (PIC, View, Palette, Text, Message, Vocabulary, Sound)
- Script AST, parser, compiler, and decompiler
- Compression algorithms (LZW, DCL, STACpack)
- Validation and version detection services

### Companion.Application
Orchestrates domain operations and provides application services:
- Project and workspace management
- Command system with undo/redo
- Document lifecycle and autosave
- Search, IntelliSense, and class browser
- External tool integration (interpreters, plugins)

### Companion.Infrastructure
Platform-specific implementations:
- File I/O and virtual file systems
- Native codec bridges (audio processing)
- Cross-platform process management
- Persistence (JSON/YAML serialization)

### Companion.App (MAUI)
User interface layer:
- Cross-platform shell with docking, navigation, and theming
- Resource editors (PIC, View, Script, Audio, etc.)
- Output panes, settings, and diagnostics
- MVVM patterns with data binding

## Current Status (As of September 2025)

### Completed Milestones

#### M0 – Project Initiation (Weeks 0-2) ✅
- Established .NET 8 MAUI solution with layered projects
- Configured dependency injection, logging, and CI pipeline
- Cataloged sample SCI0/SCI1.1 assets for testing
- Validated clean-room scope and licensing approach

#### M1 – Core Project Services (Weeks 2-6) ✅
- Implemented project metadata models and .sciproj persistence
- Built resource discovery against SCI resource maps
- Added version detection heuristics
- Delivered CLI smoke utility for project inspection

### In-Progress Work

#### M2 – Resource Serialization Core (Weeks 6-14) 🔄
**Current Focus**: Establishing the codec architecture and implementing core resource serializers.

**Completed Subtasks**:
- Defined IResource abstraction and codec registry
- Implemented Palette, Text, Message, Vocabulary, and Sound codecs
- Basic View codec metadata (full decoding pending)
- Compression infrastructure foundation

**Active Development**:
- PIC codec implementation (vector commands, layer rendering)
- Compression algorithms (LZW variants, DCL, reorder transformations)
- Golden file and regression testing setup

**Key Deliverables**:
- Full codec implementations for all resource types
- Property-based and snapshot regression tests
- Integration with resource repository

#### Compression Subsystem (Parallel to M2) 🔄
**Status**: Core algorithms in development
- LZW and LZW_1 decoders implemented
- PIC-specific reorder transformations in progress
- DCL decoder for SCI1.x underway
- Registry wiring and testing fixtures being established

#### PIC Subsystem (Parallel to M2) 🔄
**Status**: Decoder core and rendering foundation
- Opcode definitions and state machine ported
- Parser producing command lists and plane buffers
- Rendering pipeline with Skia integration
- CLI tooling for visualization and inspection

### Upcoming Milestones

#### M3 – Script System MVP (Weeks 10-20)
**Objectives**:
- Implement SCI script grammar, parser, and immutable AST
- Build semantic analyzer with scoping and type checking
- Create p-machine bytecode generator with optimizations
- Provide CLI compiler for validation and regression testing

**Key Components**:
- Parser for SCI scripting language
- Compiler targeting SCI bytecode
- Decompiler for reverse engineering
- Comprehensive test suite comparing to legacy outputs

#### M4 – Editor Framework (Weeks 18-28)
**Objectives**:
- Implement document service with autosave and undo/redo
- Create resource explorer with filtering and search
- Build script editor with syntax highlighting and IntelliSense
- Establish MVVM patterns and event messaging

**Deliverables**:
- Core editing infrastructure
- Basic resource browsing UI
- Script editing capabilities
- Output panes for diagnostics

#### M5 – Graphics Editors (Weeks 26-38)
**Objectives**:
- Deliver PIC editor with drawing tools and layer previews
- Implement raster/animation editor with timeline controls
- Build palette management with quantization
- Add onion skinning and fake ego overlays

**Features**:
- Vector-based PIC editing
- Sprite animation workflows
- Color palette tools
- Visual composition aids

#### M6 – Audio & Lip-Sync (Weeks 34-42)
**Objectives**:
- Integrate audio waveform editor and loop controls
- Port phoneme estimation pipeline cross-platform
- Implement lip-sync timing and validation
- Add automated testing with sample fixtures

**Components**:
- Audio resource manipulation
- Cross-platform speech processing
- Timing table management
- Phoneme mapping UI

#### M7 – Build & Run Integration (Weeks 38-46)
**Objectives**:
- Implement interpreter profiles (DOSBox, ScummVM, Custom)
- Provide run/debug commands with monitoring
- Build configuration UI for launch settings
- Add process management and error reporting

**Capabilities**:
- Multi-interpreter support
- Game launching workflows
- Debug integration
- Platform-specific path handling

#### M8 – Plugin Platform (Weeks 44-52)
**Objectives**:
- Define managed plugin contract and manifest
- Implement discovery, sandboxing, and invocation
- Create sample plugin and documentation
- Ensure API stability and compatibility

**Architecture**:
- Plugin hosting framework
- Security and isolation
- Extension points
- Developer tooling

#### M9 – UX Polish & Accessibility (Weeks 48-56)
**Objectives**:
- Finalize settings service and layout persistence
- Add localization and accessibility compliance
- Implement telemetry and diagnostics
- Address usability feedback

**Improvements**:
- Dockable UI framework
- Internationalization
- Accessibility standards
- User experience refinements

#### M10 – Release Stabilization (Weeks 54-60)
**Objectives**:
- Execute full regression suite
- Resolve blockers and migration concerns
- Prepare documentation and packaging
- Final quality assurance

**Activities**:
- End-to-end testing
- Performance optimization
- Documentation completion
- Release preparation

## Technical Implementation Details

### Resource System
SCI games store resources in compressed archives with a resource.map index file. Resources include:
- **PIC**: Vector-based room backgrounds with three layers (visual, priority for depth, control for walkable areas)
- **VIEW**: Animated sprites with loops (directions) and cels (frames)
- **PALETTE**: Color tables supporting palette cycling effects
- **TEXT/MESSAGE**: Localized dialog and string tables
- **VOCABULARY**: Word lists for parser input
- **SOUND**: Digital audio and MIDI sequences
- **SCRIPT**: Compiled bytecode for game logic

The rewrite implements codecs for each type, handling compression (LZW variants, DCL, STACpack) and serialization.

### Script System
SCI uses a custom scripting language compiled to p-machine bytecode. The system includes:
- Parser and AST for the scripting language
- Semantic analysis and optimization
- Bytecode generation targeting SCI runtime
- Decompiler for existing game analysis

### Compression Algorithms
Critical for resource efficiency in the era's storage constraints:
- **LZW**: Dictionary-based compression used in SCI0
- **LZW_1**: Variant with different token handling for SCI1
- **DCL**: Advanced compression for SCI1.x resources
- **Reorder**: PIC-specific transformations post-decompression

### Testing Strategy
- **Unit Tests**: Domain logic, algorithms, parsers
- **Integration Tests**: End-to-end resource pipelines
- **Golden File Tests**: Regression against known outputs
- **Property-Based Tests**: Edge cases and invariants
- **Snapshot Tests**: UI and rendering verification
- **Performance Benchmarks**: Decode/encode throughput

## Development Environment

### Prerequisites
- .NET 8 SDK with MAUI workloads
- Platform-specific dependencies (Gtk on Linux, Xcode on macOS)
- Access to sample SCI assets in repository

### Build Process
```bash
# Restore dependencies
dotnet restore Companion.Maui/Companion.Maui.sln

# Build solution
dotnet build Companion.Maui/Companion.Maui.sln

# Run CLI tool
dotnet run --project Companion.Maui/src/Companion.Cli/Companion.Cli.csproj -- TemplateGame/SCI0

# Run MAUI app (platform-specific)
dotnet run --project Companion.Maui/src/Companion.App/Companion.App.csproj -f net8.0-windows
```

### CI/CD
- GitHub Actions/Azure Pipelines with cross-platform runners
- Automated testing on all target platforms
- Code coverage and static analysis requirements

## Risks and Mitigation

### Technical Risks
- **Compression Algorithm Parity**: Complex reverse-engineered algorithms may have edge cases. *Mitigation*: Extensive golden file testing against legacy outputs, cross-reference with ScummVM implementations.
- **Cross-Platform Rendering**: Skia/Maui differences in graphics handling. *Mitigation*: Pixel-perfect comparison tests, tolerance thresholds for acceptable variations.
- **Script Compiler Fidelity**: Achieving byte-identical bytecode generation. *Mitigation*: Regression tests against known compilations, staged delivery with fallback to legacy compiler.

### Project Risks
- **Scope Creep**: Feature parity with legacy tool is extensive. *Mitigation*: Clear milestone boundaries, modular architecture allowing incremental delivery.
- **Platform Compatibility**: MAUI preview status on Linux. *Mitigation*: Regular validation on all platforms, fallback strategies.
- **Team Capacity**: Parallel workstreams require coordination. *Mitigation*: Clear ownership, regular integration points.

### External Dependencies
- **Licensing**: Clean-room rewrite to avoid GPL contamination. *Mitigation*: Information barriers, independent implementation.
- **Sample Assets**: Reliance on provided SCI fixtures. *Mitigation*: Comprehensive fixture set committed to repo.

## Success Criteria

### Functional Completeness
- All resource types editable with round-trip fidelity
- Script compilation matching legacy bytecode
- Game launching through all supported interpreters
- Plugin ecosystem functional

### Quality Metrics
- Code coverage ≥80% for domain layer
- All automated tests passing on CI
- Performance within 2x of legacy tool
- Accessibility compliance (WCAG 2.1 AA)

### User Experience
- Native feel on each platform
- Intuitive workflows for game development
- Comprehensive documentation and tutorials
- Stable releases with migration support

## Conclusion

The SCICompanion rewrite represents a significant modernization effort, bringing a beloved development tool to new platforms while preserving its rich feature set. By following TDD practices and clean architecture principles, we're building a maintainable foundation for future enhancements and extended SCI support.

The project is currently in the resource serialization phase, with solid progress on core infrastructure. The roadmap provides a clear path through MVP to full feature completion, with careful attention to quality, compatibility, and user experience.

This plan will be updated as milestones are completed and new insights emerge from development.