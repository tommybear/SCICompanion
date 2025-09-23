# Milestones

## M0 – Project Initiation (Weeks 0-2)
**Objectives**
- Validate clean-room scope, platform targets, licensing constraints.
- Spin up .NET 8 MAUI solution skeleton with layered projects (App, Application, Domain, Infrastructure).
- Establish CI pipeline, dependency injection, configuration, and logging baseline.
- Catalog sample SCI0/SCI1.1 assets for fixture-driven testing.

**Entry Criteria**
- Stakeholder agreement on scope and deliverables.

**Exit Criteria**
- Solution builds on supported platforms.
- CI runs lint + placeholder tests.
- Shared asset repository accessible to team.

## M1 – Core Project Services (Weeks 2-6)
**Objectives**
- Implement project metadata models and `.sciproj` persistence with round-trip tests.
- Deliver resource discovery against SCI resource maps and version detection heuristics.
- Ship CLI smoke utility that opens projects and enumerates resources.

**Exit Criteria**
- Loading/saving sample projects succeeds with automated tests.
- Version detection reports expected interpreter targets for fixtures.
- CLI tool exercised in CI.

## M2 – Resource Serialization Core (Weeks 6-14)
**Objectives**
- Define `IResource` abstraction and codec registry.
- Implement codecs for PIC, View/Animation, Palette, Text/Message, Vocabulary, and Sound resources.
- Add golden file, property-based, and snapshot regression tests.

**Exit Criteria**
- Import/export parity for curated fixtures (SCI0 & SCI1.1) with checksums.
- Failure cases return actionable diagnostics.
- Code coverage for domain layer ≥80%.

## M3 – Script System MVP (Weeks 10-20)
**Objectives**
- Implement grammar, parser, immutable AST, and semantic analyzer.
- Build p-machine bytecode generator with optimization hooks.
- Provide CLI compiler for script compilation and regression comparison to legacy outputs.

**Exit Criteria**
- Scripts from template games compile with matching bytecode hashes (allowing known diffs).
- Unit and integration suites validating language features are green.
- CLI compiler packaged for internal use.

## M4 – Editor Framework (Weeks 18-28)
**Objectives**
- Create document service, undo/redo stack, autosave, and messaging bus.
- Ship resource explorer view, output panes, and script editor with syntax highlighting + IntelliSense skeleton.
- Establish MVVM patterns across MAUI front end.

**Exit Criteria**
- Editing round-trip (open → edit → save) validated for scripts and text resources.
- Undo/redo scenarios covered by automated tests.
- MAUI app usable for basic browsing/editing flows without crashes.

## M5 – Graphics Editors (Weeks 26-38)
**Objectives**
- Deliver PIC editor with drawing tools, layer previews, onion skin/fake ego overlays.
- Implement raster/animation editor with timeline, palette selection, hit testing.
- Provide palette management UI including quantization utilities.

**Exit Criteria**
- Visual edits persist correctly when opened in legacy SCI Companion (characterization check).
- Snapshot regression tests for rendering pipelines are stable.
- Performance targets met for large backgrounds (≤200 ms redraw).

## M6 – Audio & Lip-Sync (Weeks 34-42)
**Objectives**
- Integrate audio waveform editor, loop controls, and metadata editing.
- Port phoneme estimation/lip-sync pipeline with cross-platform abstraction.
- Add automated verification using provided sample audio fixtures.

**Exit Criteria**
- Lip-sync timelines generated within tolerance of legacy tool output.
- Audio resources play and serialize correctly in interpreter smoke tests.
- UI supports manual phoneme adjustments with undo/redo.

## M7 – Build & Run Integration (Weeks 38-46)
**Objectives**
- Implement interpreter profiles (DOSBox, ScummVM, Custom) with platform-specific path resolution.
- Provide run/debug commands with process monitoring and error reporting.
- Expose configuration UI for per-project launch settings.

**Exit Criteria**
- Launching template games succeeds on Windows/macOS/Linux with configured profiles.
- Command-line assembly covered by automated tests.
- Error states surfaced in output pane with actionable guidance.

## M8 – Plugin Platform (Weeks 44-52)
**Objectives**
- Define managed plugin contract and metadata manifest format.
- Implement discovery, sandboxing, and invocation with project context.
- Publish sample plugin and developer documentation.

**Exit Criteria**
- Plugins load/unload dynamically without impacting IDE stability.
- Compatibility tests guard API evolution.
- Documentation reviewed by prospective plugin authors.

## M9 – UX Polish & Accessibility (Weeks 48-56)
**Objectives**
- Finalize settings service, dockable layout persistence, localization, and accessibility.
- Add telemetry/logging dashboards for diagnostics.
- Address usability feedback gathered during internal dogfooding.

**Exit Criteria**
- Accessibility checklist (keyboard nav, contrast, screen reader labels) satisfied.
- Settings persist across sessions and sync with project overrides.
- Localization coverage meets target languages.

## M10 – Release Stabilization (Weeks 54-60)
**Objectives**
- Execute full regression suite (unit, integration, snapshot, UI smoke, acceptance scenes).
- Resolve open risk items: distribution packaging, migration tooling, decompiler scope decision, lip-sync dependency health.
- Prepare release notes, onboarding docs, contribution guidelines.

**Exit Criteria**
- All blocking defects triaged and resolved or deferred with mitigation.
- Installers/packages validated on supported platforms.
- Documentation complete and published.
