# SCI Companion MAUI Clean-Room Rewrite

## 1. Purpose and Scope
This document captures the technical design and test-driven development (TDD) strategy for a clean-room reimplementation of SCI Companion as a cross-platform .NET MAUI application. It is informed by the current C++/MFC codebase (notably `SCICompanionLib/Src`) and aims to preserve feature breadth while modernizing architecture, tooling, and user experience. The rewrite targets desktop platforms (Windows, macOS, Linux); mobile form factors are out of scope for the initial release.

## 2. Background Research Summary
- Existing solution is a Windows-only MFC IDE with the bulk of logic consolidated under `SCICompanionLib` (resource management, compiler, decompiler, editor logic, project services) and a thin shell in `SCICompanion` (`README.md`, `SCICompanion/SCICompanion.cpp`).
- Resource editing spans graphics (PIC command sequences, raster views, palette operations), animation, fonts, text/message tables, vocabulary resources, and audio (`SCICompanionLib/Src/Resources`).
- Script workflows cover a custom compiler, decompiler, syntax services, IntelliSense, and class browser functionality (`SCICompanionLib/Src/Compile`, `SCICompanionLib/Src/Util/AppState.h`).
- Runtime integration launches DOSBox or ScummVM with auto-discovered executables and ini-based configuration (`SCICompanionLib/Src/Util/RunLogic.cpp`).
- Ancillary utilities include lip-sync generation (SAPI-based), GIF export, template game creation, plugin invocation of external executables, and bundled tools.

## 3. Goals and Non-Goals
### Goals
- Deliver feature parity (or deliberate supersets) for SCI0–SCI1.1 resource creation, editing, compilation, and playtesting.
- Architect a maintainable, testable, cross-platform codebase using modern .NET patterns.
- Embrace TDD to ensure correctness of domain logic, serialization, and compilation pipelines.
- Provide a modular foundation that supports future extensibility (additional SCI versions, new editors, enhanced tooling).

### Non-Goals (Initial Release)
- Mobile device support.
- Real-time in-app game emulation; focus on launching external interpreters.
- Feature-level parity for all niche MFC behaviors (e.g., exact docking chrome); aim for analogous UX using MAUI paradigms.
- Maintaining binary compatibility with legacy plugins; design a new, managed add-in model.

## 4. User Roles and Primary Scenarios
- **Game Designer:** Navigates resources, edits graphics/scripts/audio, compiles, and iterates rapidly.
- **Programmer:** Authors SCI scripts, relies on syntax highlighting, IntelliSense, compiler diagnostics, and version control friendly outputs.
- **Artist:** Focuses on raster/palette editing, onion skinning, fake ego tracing aids, export tooling.
- **Audio Designer:** Imports/edits sound resources, configures loop points, performs lip-sync alignment.
- **Plugin Developer:** Extends IDE via managed add-ins invoked with project context.

Key flows: project creation from template, resource browsing, script editing/compilation, graphics editing, audio management, build and run, plugin invocation.

## 5. High-Level Architecture
```
+-------------------------------------------------------------+
|                     Companion.App (MAUI)                    |
|  - UI Shell (Windowing, Navigation, Docking)                |
|  - Tools: Editors, Explorer, Output panes, Settings         |
+----------------------------|--------------------------------+
                             |
+----------------------------v--------------------------------+
|                 Companion.Application (Services)            |
|  - Project/Workspace Service                                |
|  - Command System & Undo/Redo                               |
|  - Document Lifecycle & Autosave                            |
|  - Intellisense / Class Browser / Search                    |
|  - External Tool & Emulator Integration                     |
|  - Plugin Host                                              |
+----------------------------|--------------------------------+
                             |
+----------------------------v--------------------------------+
|                 Companion.Domain (Core Libraries)           |
|  - Resource Models & Serialization                          |
|  - Script AST, Parser, Compiler, Decompiler                 |
|  - Rendering Pipelines (PIC command interpreter, raster)    |
|  - Audio Processing & Lip-sync abstraction                  |
|  - Validation & Version Detection                           |
+----------------------------|--------------------------------+
                             |
+----------------------------v--------------------------------+
|           Companion.Infrastructure (Platform Abstractions) |
|  - File IO, Virtual File Systems                            |
|  - Native codec bridges (e.g., audio decoding)              |
|  - Cross-platform process management                        |
|  - Persistence (SQLite, JSON/YAML)                          |
+-------------------------------------------------------------+
```

### Layering Principles
- UI is dumb: interacts through view models and commands; no resource parsing in MAUI layer.
- Domain layer is platform-agnostic .NET (netstandard2.1+/net8) with pure logic and TDD coverage.
- Application services orchestrate domain operations, manage documents, enforce workflows, and provide event buses for UI updates.
- Infrastructure injects platform specifics via interfaces (file dialogs, clipboard, speech APIs) with MAUI dependency injection.

## 6. Module Designs
### 6.1 Project & Workspace Service
- **Responsibilities:** Manage SCI project metadata (`.sciproj`), resource map (`resource.map`), patch directories, dependency tracking, version detection.
- **Data Model:**
  - `ProjectMetadata` (name, version, template id, interpreter profile).
  - `ResourceDescriptor` (type, number, package, source flags, variant info).
  - `WorkspaceState` (open documents, layout, MRU).
- **Persistence:** YAML or JSON for project metadata; binary compatibility maintained for exportable SCI resources.
- **Interfaces:**
  - `IProjectLoader`, `IResourceRepository`, `IVersionDetector`.
- **TDD Strategy:**
  - Fixture-based tests verifying load/save round-trips for sample projects.
  - Resource discovery tests covering SCI0 vs SCI1.1 map formats.
  - Version detection heuristics with curated resource sets.

### 6.2 Resource Model & Serialization
- **Scope:** Replace `ResourceEntity`/components with idiomatic domain models (PicResource, ViewResource, PaletteResource, etc.).
- **Design:**
  - Each resource type implements `IResource` with metadata and payload.
  - Serialization handled by pluggable `IResourceCodec` instances.
  - Maintain component-based composition for shared concerns (raster frames, palettes, control lines).
- **Compatibility:** Validate against original interpreter expectations and existing SCI assets.
- **TDD:**
  - Golden file tests using extracted resources from sample games.
  - Property-based tests for encode/decode idempotence.
  - Error handling tests for malformed resource streams.

### 6.3 Script System
- **Parser & AST:**
  - Implement LL/LR parser or Pratt parser in managed code; model AST nodes akin to `ScriptOM` but immutable.
  - Provide syntax tree services for navigation, formatting, and static analysis.
- **Compiler:**
  - Recreate p-machine bytecode target, bridging binary operator mapping (`Compile.cpp`) and runtime calling conventions.
  - Build pipeline stages: syntax analysis, semantic analysis, code generation, optimization passes.
- **Decompiler:**
  - Translate bytecode back to high-level constructs for diffing and template generation.
- **Services:**
  - IntelliSense (symbol tables, method signatures) and class browser support.
- **TDD:**
  - Grammar tests using canonical SCI scripts.
  - Semantic validation tests (scope rules, reserved keywords).
  - Codegen verification comparing output to known-good binaries.
  - Round-trip compile/decompile tests to ensure fidelity.
  - Performance benchmarks integrated into CI (not blocking but monitored).

### 6.4 Graphics Pipeline (PIC, Views, Palettes)
- **Rendering:**
  - Implement PIC command interpreter generating layer buffers (visual, priority, control) similar to `PicDrawManager`.
  - Use MAUI `GraphicsView` with Skia renderer for cross-platform drawing.
- **Editing Tools:**
  - Command stack with undo/redo, brush tools, polygon editing, onion skin overlays (fake ego pathing).
  - Raster view editor for loops/cels with timeline, palette selection, hit testing.
  - Palette editor with preview, quantization utilities.
- **TDD:**
  - Unit tests for PIC command execution output (pixel buffer comparisons).
  - Palette conversion tests ensuring color mapping invariants.
  - Snapshot-based rendering regression tests using reference PNGs (allow small tolerances).

### 6.5 Text, Message, Vocabulary Editors
- **Message/Text:** Provide table editors with multi-language columns, talker id management, import/export to CSV/JSON.
- **Vocabulary:** Manage word groups, synonyms, cases as in `NounsAndCases.cpp`.
- **TDD:**
  - Data structure tests for case mapping and validation.
  - Import/export round-trip tests.

### 6.6 Audio & Lip-Sync
- **Audio Resource Handling:** Support SCI sound formats (digital, MIDI) with decode/encode support.
- **Lip-Sync:** Abstract SAPI dependency; integrate cross-platform phoneme estimation (native port of existing algorithms or third-party library).
- **TDD:**
  - Tests verifying timing tables, sync markers, and wave loop handling.
  - Lip-sync mapping tests using sample audio/phoneme fixtures (`Samples/PhonemeSentence.*`).

### 6.7 Game Explorer & Search
- Tree and tile views for resources, filters by type/state, quick search, dependency graph visualization.
- Integrate resource recency similar to `_resourceRecency` data from `AppState`.
- **TDD:** UI logic tested via view model unit tests and snapshot state verification.

### 6.8 Build & Run Integration
- Cross-platform `IInterpreterProfile` abstraction for DOSBox, ScummVM, or custom executables.
- Provide configuration UI to specify install paths per platform.
- Support per-game ini generation mirroring `RunLogic` behavior.
- **TDD:**
  - Tests for command-line assembly per profile.
  - Path detection tests using sandboxed fake file systems.

### 6.9 Plugin System
- Managed plugin host loading assemblies from `Plugins/<Name>/<Name>.dll` with optional metadata manifest.
- Provide sandboxed API surface (`ICompanionPlugin` with `OnInvoke(ProjectContext)` etc.).
- Security: optional signing, user consent prompts.
- **TDD:**
  - Plugin loading tests with mocks verifying isolation.
  - Contract tests ensuring API version compatibility.

### 6.10 Settings, Layout, and UX Shell
- Extensible settings system (JSON) with per-project overrides.
- Dockable layout using MAUI multi-window + custom docking control (or third-party component) with persistence.
- Localization support via `.resx` or `.po` resources.
- Accessibility compliance (keyboard navigation, high contrast, screen reader hints).
- **TDD:**
  - Settings serialization tests.
  - Layout restoration tests via view model serialization.

## 7. Cross-Cutting Concerns
- **Undo/Redo:** Command-based history shared across editors; tests verifying transactional integrity.
- **Eventing:** Event aggregator for resource changes, compile status, diagnostics.
- **Diagnostics:** Structured logging (Serilog) with per-project log files, diagnostic console.
- **Error Handling:** Domain-specific exceptions with user-facing remediations.
- **Performance:** Profiling hooks, lazy loading of large resources, background scheduling for heavy operations (compilation, image recompute).

## 8. Technology Choices
- .NET 8, MAUI desktop profile.
- Dependency injection via built-in `Microsoft.Extensions.DependencyInjection`.
- Task-based async operations; background scheduling with `IBackgroundTaskQueue` abstraction.
- Serialization: `System.Text.Json` plus binary custom serializers.
- Testing frameworks: `xUnit` (unit), `Verify` (snapshot), `Playwright` or `Appium` for automated UI smoke.
- Build system: `dotnet` CLI, GitHub Actions/Azure Pipelines for CI, cross-platform runners.

## 9. TDD Strategy and Test Matrix
### 9.1 Guiding Principles
- Red-green-refactor for all domain features.
- Favor deterministic, side-effect free tests; wrap IO in interfaces to enable mocking.
- Maintain extensive sample fixtures committed to repo for regression detection.
- Blend unit, integration, and characterization tests to guard compatibility with legacy assets.

### 9.2 Test Layers
- **Domain Unit Tests:** Resource codecs, AST transformations, compiler stages, palette math.
- **Integration Tests:** Resource pipeline end-to-end (import -> edit -> export), compiler -> interpreter compatibility checks.
- **Contract Tests:** Plugin API surface, interpreter profile handshake.
- **UI Interaction Tests:** Focused on high-value flows (open project, edit script, run compile) using MVVM-level tests plus targeted automated UI suites.
- **Performance Benchmarks:** Non-blocking tests measuring compile throughput, large resource handling.
- **Acceptance Tests:** Scenario scripts executed against nightly builds using representative sample games.

### 9.3 Test Data Assets
- Curated SCI0 and SCI1.1 sample games (from `Samples/SCI0`, `Samples/SCI1.1`).
- Extracted golden resources stored under `tests/Fixtures` with checksum validation.
- Script corpus covering language edge cases, macro usage, message tables, vocabulary variants.
- Audio fixtures for lip-sync and sound resource validation.

### 9.4 Tooling and Automation
- `dotnet test` with code coverage thresholds enforced per module.
- PR gate requiring passing unit/integration suites and static analysis (StyleCop, Sonar optional).
- Nightly triggered acceptance jobs executing end-to-end smoke flows on all supported platforms.
- Test reporting integrated with CI (TRX/JUnit exports).

## 10. Project Roadmap (Milestones)
1. **Foundation (Weeks 0-6)**
   - Establish solution skeleton, DI, base infrastructure.
   - Implement project loader, resource metadata models.
   - Write characterization tests against legacy resource files.

2. **Resource Serialization Core (Weeks 6-14)**
   - Implement codecs for PIC, View, Palette, Text, Message, Sound.
   - Validate via golden file tests.

3. **Script System MVP (Weeks 10-20 overlap)**
   - Deliver parser, AST, semantic checks, partial code generator.
   - Provide CLI-based compiler for early adopters.

4. **Editor Framework (Weeks 18-28)**
   - Build document service, undo/redo, MVVM patterns.
   - Ship initial editors (Script, Resource Explorer, Palette).

5. **Graphics Editors (Weeks 26-38)**
   - Implement PIC and raster editors with core tooling.
   - Integrate onion skin/fake ego visualization.

6. **Audio & Lip-sync (Weeks 34-42)**
   - Port audio resource manipulation and lip-sync pipeline.

7. **Interpreter Integration & Build (Weeks 38-46)**
   - Implement interpreter profiles, run/debug workflows.

8. **Plugin & Extensibility (Weeks 44-52)**
   - Deliver managed plugin SDK and sample extensions.

9. **Release Stabilization (Weeks 50-60)**
   - Harden cross-platform UX, telemetry, accessibility, documentation.
   - Execute full regression suite and acceptance criteria.

## 11. Risk Assessment and Mitigations
- **Complex Resource Formats:** Mitigate via characterization tests and collaborative reverse engineering notes.
- **Compiler Parity:** Stage delivery, begin with subset of language, leverage existing scripts for regression; consider embedding legacy compiler via interop for validation during transition.
- **Cross-Platform Rendering:** Prototype Skia-based editors early; allocate buffer for performance tuning.
- **Lip-Sync Cross-Platform Dependencies:** Evaluate open-source phoneme extractors; define fallback manual editing workflow.
- **Team Capacity:** Modular milestone plan enabling parallel workstreams (domain vs UI vs tooling).

## 12. Open Questions
- What is the distribution strategy (MSIX, PKG, AppImage)?
- Should we support legacy patch folder layout or introduce new structure with migration tooling?
- How far do we go in replicating Prof-UIS docking behavior vs adopting a simplified MAUI layout?
- Do we maintain decompiler or treat as secondary milestone post-MVP?

## 13. Appendices
### 13.1 Reference Materials Consulted
- `README.md` for high-level scope.
- `SCICompanion/SCICompanion.cpp` for application wiring, command surface.
- `SCICompanionLib/Src/Util/AppState.h` for global services and editor template definitions.
- `SCICompanionLib/Src/Compile/Compile.cpp` and related headers for compiler behavior.
- `SCICompanionLib/Src/Util/RunLogic.cpp` for interpreter integration design.
- `SCICompanionLib/Src/Resources/*` for resource type coverage.
- `Plugins/Readme.txt` for plugin interaction model.

### 13.2 Glossary
- **SCI:** Sierra Creative Interpreter, the runtime for classic Sierra adventure games.
- **PIC:** Vector-based background resource used by SCI.
- **View:** Sprite/animation resource with loops and cels.
- **Message Table:** Multi-language dialog/resource string table.
- **Fake Ego:** Cursor/preview of player character for testing visual composition.
- **Resource Map:** Index of resources in SCI game packages.

