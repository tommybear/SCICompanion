# TODO

## Immediate Setup
- [x] Confirm project scope, platform targets, and licensing for clean-room rewrite
- [x] Establish .NET 8 MAUI solution skeleton with layered projects (App, Application, Domain, Infrastructure)
- [x] Wire up dependency injection, logging, configuration, and initial CI scaffolding
- [x] Collect and catalog sample SCI0/SCI1.1 assets for fixture-based testing

## Foundation (Weeks 0-6)
- [x] Implement project metadata models (`ProjectMetadata`, `ResourceDescriptor`)
- [x] Build JSON loader & writer for new `.sciproj` format with round-trip tests (YAML pending)
- [x] Implement resource discovery against SCI resource maps with characterization tests
- [x] Port version detection heuristics with sample coverage
- [x] Deliver CLI smoke tool for project loading to inform early consumers

## Resource Serialization Core (Weeks 6-14)
- [x] Define `IResource` abstraction and codec registry
- [ ] Implement PIC codec (commands, visual/priority/control layers) with golden files
- [x] Implement View codec metadata placeholder (full decoding pending)
- [x] Implement Palette codec with validation tests
- [x] Implement Text codec with validation tests
- [x] Implement Message codec with validation tests
- [x] Implement Vocabulary codec with validation tests
- [x] Implement Sound codec with validation tests (future compression work pending)
- [ ] Introduce property-based and snapshot regression tests for resource pipelines

## Script System MVP (Weeks 10-20)
- [ ] Define grammar and implement parser + immutable AST
- [ ] Add semantic analyzer (scoping, type checks, reserved keywords)
- [ ] Implement bytecode generator targeting SCI p-machine instructions
- [ ] Build red-green suite comparing compiled output against legacy binaries
- [ ] Provide CLI compiler (netstandard) for early validation
- [ ] Plan decompiler parity tests (stage for later milestone)

## Editor Framework (Weeks 18-28)
- [ ] Implement document service, autosave, and command-based undo/redo
- [ ] Create resource explorer tree with filters & MRU support
- [ ] Ship script editor (syntax highlighting, IntelliSense stubs)
- [ ] Integrate output panes for diagnostics and compile results
- [ ] Establish MVVM patterns and messaging/event aggregator

## Graphics Editors (Weeks 26-38)
- [ ] Implement PIC editor using MAUI `GraphicsView`/Skia renderer with toolset
- [ ] Implement raster editor for views (timeline, onion skin, hit testing)
- [ ] Build palette editor UI with quantization and preview controls
- [ ] Recreate fake ego visualization & aspect ratio handling
- [ ] Add snapshot-based rendering regression tests

## Audio & Lip-Sync (Weeks 34-42)
- [ ] Integrate audio waveform visualizer and editing controls
- [ ] Port lip-sync phoneme estimation with cross-platform abstraction
- [ ] Implement timing/loop configuration UI and validation
- [ ] Add automated tests for audio metadata and lip-sync alignment

## Build & Run Integration (Weeks 38-46)
- [ ] Design interpreter profile abstraction (`DOSBox`, `ScummVM`, `Custom`)
- [ ] Implement per-profile command assembly with cross-platform path resolution
- [ ] Build configuration UI for interpreter setup and per-project overrides
- [ ] Provide run/debug commands with process monitoring & error surfacing
- [ ] Write tests simulating game folder launches using sandboxed paths

## Plugin System (Weeks 44-52)
- [ ] Define managed plugin contract and metadata manifest
- [ ] Implement plugin discovery, isolation, and lifecycle hooks
- [ ] Ship sample plugin and developer documentation
- [ ] Add compatibility tests for plugin API evolution

## Settings, UX Shell, and Accessibility
- [ ] Implement settings service with global/project overrides
- [ ] Deliver dockable layout solution with persistence
- [ ] Localize core UI and ensure keyboard navigation/accessibility compliance
- [ ] Add telemetry/logging dashboard for diagnostics

## Cross-Cutting Quality Gates
- [ ] Configure automated test matrix (unit, integration, snapshot, UI smoke)
- [ ] Enforce coverage thresholds and static analysis in CI
- [ ] Add performance benchmarks and tracking dashboards
- [ ] Document contribution guidelines and coding standards

## Risk Follow-Up
- [ ] Decide on distribution strategy (MSIX/PKG/AppImage) and automation
- [ ] Finalize migration path for existing projects and patch folders
- [ ] Evaluate feasibility of legacy decompiler parity before 1.0 launch
- [ ] Assess availability of cross-platform lip-sync dependencies

