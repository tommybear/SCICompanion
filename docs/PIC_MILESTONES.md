# Milestone – Complete PIC Functionality

## Phase 1: Compression Infrastructure (Weeks 0–2)
**Objectives**
- Implement SCI compression algorithms (LZW, LZW_1, LZW_Pic reorder, DCL, optional STACpack).
- Integrate services into `CompressionRegistry` and wire through DI.
- Establish golden decompression tests using legacy assets.

**Exit Criteria**
- All known PIC compression methods decompress to buffers matching SCI Companion outputs.
- Unit tests cover success and failure cases (unsupported method).

## Phase 2: Decoder Core (Weeks 2–6)
**Objectives**
- Port opcode definitions and state machine for SCI0/SCI1.x PICs.
- Implement parser producing structured command list and maintaining palette/pattern/priority state.
- Generate raster planes (visual/priority/control) during decode.

**Exit Criteria**
- Template PICs decode without errors; state snapshots validate against legacy data.
- Opcode histogram matches legacy tool output for sampled PICs.

**Status (2025-09-23)**
- Decoder/regression tests (`PicPhaseTwoValidationTests`) cover TemplateGame PICs and assert opcode histograms, satisfying Phase 2 exit gates. Additional analytics (palette usage/draw order) tracked under Phase 3 tooling.

## Phase 3: Rendering & Analytics (Weeks 4–8)
**Objectives**
- Finalize `PicDocument` model (commands + planes + metadata).
- Implement renderer/exporter creating PNG/BMP outputs for visual/priority/control.
- Add CLI tooling to visualize and inspect decoded scenes.

**Status (2025-09-23)**
- `PicDocument` and the rasterizer now drive palette-aware PNG/BMP exports through the CLI, including legends for priority/control planes. Export analytics and automated comparisons remain.

**Exit Criteria**
- Visual comparison (pixel diff within tolerance) between rendered outputs and legacy renders for SCI0/SCI1.1 templates.
- CLI can export planes and show state info for arbitrary PICs.

## Phase 4: Encoder & Round-Trip (Weeks 6–10)
**Objectives**
- Serialize `PicDocument` back into opcode stream preserving semantics.
- Reapply compression and ensure byte-identical round-trip where edits are neutral.
- Provide diagnostics for unsupported edits.

**Exit Criteria**
- Template PICs round-trip byte-for-byte through decoder→encoder pipeline.
- Regression tests catch deviations and report actionable errors.

## Phase 5: QA & Performance (Weeks 8–12)
**Objectives**
- Add property-based tests for random command sequences.
- Benchmark decode/encode performance; optimize critical paths.
- Document architecture, edge cases, and migration notes from legacy code.

**Exit Criteria**
- Property tests run in CI without failures; benchmarks meet agreed performance targets.
- Documentation published (PIC_TDD, developer guide, CLI usage).

## Risks & Mitigations
- **Compression parity**: Validate against legacy outputs; consider reusing algorithm references from ScummVM.
- **Rendering discrepancies**: Validate planes via diff tools; add tolerance thresholds and highlight mismatches in tests.
- **Encoder fidelity**: Use deterministic ordering and state snapshots; flag divergence early via golden tests.

## Dependencies
- Access to SCI template assets (`TemplateGame/SCI0`, `SCI1.1`).
- Legacy SCI Companion code for reference.
- Skia/Graphics stack (for exports) available in build environment.
