# SCICompanion Project Plan

## Overview

SCICompanion is a clean-room rewrite of the classic SCI Companion toolchain, bringing IDE, asset, and script authoring support for Sierra Creative Interpreter (SCI0–SCI1.1) games to a modern, cross-platform .NET 8 MAUI stack. The solution currently focuses on foundational project services and resource inspection while laying groundwork for editors, compilers, and extensibility.

## Supporting References
- `MILESTONES.md` — canonical milestone definitions; keep the sections below aligned.
- `TODO.md` — cross-cutting backlog items that feed upcoming milestone work.
- `TDD.md` — global testing approach; expand as new test suites land.
- `docs/Compression_MILESTONES.md`, `docs/Compression_TDD.md`, `docs/Compression_TODO.md` — detailed compression roadmap, test criteria, and backlog.
- `docs/PIC_MILESTONES.md`, `docs/PIC_TDD.md`, `docs/PIC_TODO.md` — PIC renderer/codec breakdown and open items.
- `Reference_Deprecated_Project/` — read-only legacy implementation for behaviour parity research.

## Project Goals

### Primary Objectives
- **Cross-Platform Compatibility**: Deliver a native desktop experience on Windows 11+, macOS 14+, and Ubuntu 22.04+ via .NET 8 MAUI.
- **Modern Architecture**: Maintain clean separation of Domain, Application, Infrastructure, and UI layers with testable boundaries.
- **Feature Parity**: Reproduce legacy SCI Companion capabilities, including lip-sync tooling and plugin extensibility.
- **Evidence-Driven Development**: Follow the testing guidance in `TDD.md`, expanding automated coverage (unit, integration, snapshot, property-based) alongside new features.
- **Extensibility**: Provide a managed plugin surface that can evolve to support future SCI formats.

### Non-Goals
- Mobile platforms (iOS/Android).
- Bundled emulation (continue delegating to DOSBox/ScummVM/etc.).
- SCI2+ resource formats.
- Binary compatibility with legacy native plugins.

## Architecture Overview

### Companion.Domain
- **Current capabilities**: Resource descriptor models, `ResourcePackage` abstractions, basic codecs (Palette/Text/Message/Vocabulary/Sound/View with header parsing), PIC command parser, and a compression registry covering passthrough/LZW/LZW_1/DCL/STACpack variants.
- **Upcoming priorities**: Finish codec round-tripping, validate DCL/STACpack implementations against fixtures (`docs/Compression_TODO.md`), implement script AST/compiler/decompiler pipelines, and derive rendering primitives for PIC/VIEW assets (`docs/PIC_TODO.md`).

### Companion.Application
- **Current capabilities**: Dependency injection wiring, sample asset catalog, JSON project metadata store, resource discovery (SCI0/SCI1/SCI1.1 map parsing), resource repository/volume reader, and CLI project inspector.
- **Upcoming priorities**: Command/undo infrastructure, document lifecycle, richer diagnostics, and integration glue for future editors (see `TODO.md`).

### Companion.Infrastructure
- **Current capabilities**: Placeholder project with DI shim.
- **Upcoming priorities**: File-system abstractions, native codec bridges, audio tooling, and process orchestration once editors and interpreter workflows begin.

### Companion.App (MAUI)
- **Current capabilities**: Bootstrapped MAUI project referencing Application/Domain/Infrastructure layers; no functional UI yet.
- **Upcoming priorities**: Establish shell, docking, theming, and begin editor surface work after M2 stabilization.

## Current Status (Updated 2025-09-23)

### Completed
- **M0 – Project Initiation**: Solution skeleton, layered project layout, dependency injection setup, initial CI config, and fixture catalog scaffolding delivered in line with `MILESTONES.md`.

### Partially Completed
- **M1 – Core Project Services**: Metadata persistence, resource discovery, CLI smoke inspector, and initial tests shipped. Outstanding items include richer version heuristics, golden fixtures, and CLI automation per `TODO.md` and `docs/Compression_MILESTONES.md`.

- **M2 – Resource Serialization Core**: Codecs exist as metadata extractors; encode paths, fidelity validation, and failure diagnostics remain. PIC parsing and rasterization now satisfy Phase 2 exit criteria with TemplateGame snapshots/histograms locked in tests, while export analytics and round-trip encoding move to Phase 3 (`docs/PIC_TODO.md`).
- **Phase 3 Tooling**: CLI exports are palette-aware and now include opcode/plane summaries (`--pic-summary`), draw-order samples, command dumps (`--pic-ops`), and pixel-diff comparisons (`--compare`, `--compare-baseline`). Baseline PNGs live under `Companion.Maui/tests/Baselines/Pic`. Remaining work: automate CI baseline checks and add re-encode diff tooling.
- **Compression Subsystem**: Passthrough, base LZW, LZW_1, DCL, STACpack, and LZW_Pic/View reorder implementations are wired with unit coverage. Fixture consolidation (real-world STACpack payloads) remains; CLI diagnostics and dump helpers are in place (`docs/Compression_MILESTONES.md`, `Reference_Deprecated_Project/SCICompanionLib`).
- **Testing Infrastructure**: xUnit unit tests exercise early services, including coverage for LZW/LZW_1, reorder pipelines, and relative line/pattern rasterization. Property-based, snapshot, performance, and golden tests remain to be built in accordance with `TDD.md`, `docs/Compression_TDD.md`, and `docs/PIC_TDD.md`.

## Near-Term Milestones & Focus
1. **Stabilize M2**
   - Finish codec round trips and validation.
   - Finish STACpack fixture coverage with real SCI2 payloads.
   - Harden the palette-aware rasterizer (default palette seeding, analytics, SCI1 bitmap opcodes) and produce inspection-ready outputs for PIC/VIEW assets.
2. **Prepare M3 – Script System MVP**
   - Define grammar/AST structure and semantic analysis skeletons.
   - Establish compiler test harness referencing fixture games (sync scope with `TODO.md`).
3. **Lay groundwork for editor framework (M4)**
   - Design document/command services and capture requirements from `docs/PIC_MILESTONES.md`.

Revisit `MILESTONES.md` after each sprint to ensure sequencing stays accurate.

## Technical Implementation Notes

### Resource System
- Current implementation supports reading SCI map/index data, projecting descriptors, and decoding payloads to metadata-rich structures for inspection. Round-trip editing is not yet guaranteed. Use `docs/PIC_TODO.md` and `docs/Compression_TODO.md` when refining codecs.

### Script System
- No managed script tooling exists yet; rely on legacy research notes inside `Reference_Deprecated_Project/`. `TODO.md` tracks prerequisites for kicking off M3.

### Compression Algorithms
- Implemented: raw passthrough (0), LZW (1), LZW_1 (2), DCL (8/18–20), STACpack (32), and LZW_Pic/View reorders (3/4).
- Outstanding: Optional encoders and fixture-backed regressions. Follow `docs/Compression_TDD.md` for acceptance criteria.

### Testing Strategy
- Current: xUnit unit suites in `Companion.Application.Tests` cover metadata store, resource discovery, codec registry, and basic codecs.
- Next steps: add golden fixtures, property-based cases, snapshot rendering comparisons, and benchmarks per `TDD.md`, `docs/Compression_TDD.md`, and `docs/PIC_TDD.md`.

## Development Environment
- Prerequisites remain `.NET 8 SDK` with MAUI workloads and platform prerequisites (Gtk/Xcode). Build instructions in this plan mirror the README; keep them synchronized when workflows change.

## Risks and Mitigation
- **Algorithm Fidelity**: Compression and PIC rendering parity are still research-heavy. Mitigate by porting tests/fixtures from legacy sources and documenting edge cases (`docs/Compression_TODO.md`, `docs/PIC_TODO.md`).
- **Testing Debt**: Lack of golden/property-based coverage risks regressions. Prioritize the testing roadmap in `TDD.md` alongside feature work.
- **Scope Management**: Ambitious parity goals require disciplined milestone gating. Regularly reconcile this plan with `MILESTONES.md` and `TODO.md`.
- **Cross-Platform MAUI**: Infrastructure/UI layers are unimplemented; schedule early smoke builds on Windows/macOS/Linux to surface platform blockers.

## Success Criteria
- End-to-end resource editing with round-trip fidelity for SCI0–SCI1.1 (verified against fixtures and, where possible, legacy outputs).
- Script compilation/decompilation matching legacy bytecode expectations.
- Integrated interpreter launch workflows with actionable diagnostics.
- Managed plugin system with documented extension points.
- Automated test coverage aligned with thresholds defined in `TDD.md`.

## Next Review
Re-evaluate this document after closing out the remaining M2 compression and codec tasks. Update milestone status, link any new design docs, and ensure parity with `MILESTONES.md` and topical docs.
