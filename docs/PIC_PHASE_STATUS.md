## PIC Phase Status – Snapshot (2025-09-23)

### Phase 1 – Compression Infrastructure

| Objective | Status | Notes |
|-----------|--------|-------|
| Implement SCI compression algorithms (LZW, LZW_1, LZW_Pic reorder, DCL, optional STACpack). | ✅ Done | See `Companion.Domain/Compression` and `CompressionFixtureTests` coverage for methods 0/1/2/3/4/18-20/32. |
| Integrate services into `CompressionRegistry` and wire through DI. | ✅ Done | Registry is injected into codecs (`PicResourceCodec`) and CLI tooling. |
| Establish golden decompression tests using legacy assets. | ✅ Done (baseline) | Fixtures for SCI0 PIC/View/Sound & SCI1.1 DCL in `CompressionFixtureTests`; still expanding fixture catalog per compression TODO. |

| Exit Criteria | Status | Notes |
|---------------|--------|-------|
| All known PIC compression methods decompress to buffers matching SCI Companion outputs. | ✅ Done | Verified via `CompressionFixtureTests`; additional fixtures planned for broader coverage. |
| Unit tests cover success and failure cases (unsupported method). | ✅ Done | Failure cases in `CompressionFixtureTests` and registry tests. |

**Outcome:** Phase 1 is satisfied; future work focuses on optional encoders and richer fixture coverage, tracked separately in `docs/Compression_TODO.md`.

### Phase 2 – Decoder Core

| Objective | Status | Notes |
|-----------|--------|-------|
| Port opcode definitions and state machine for SCI0/SCI1.x PICs. | ✅ Done | Implemented in `PicOpcode`, `PicStateMachine`, and parser records. |
| Implement parser producing structured command list and maintaining palette/pattern/priority state. | ✅ Done (baseline) | `PicParser` & `PicStateMachine`; pending: extended opcode coverage (`Sci1DrawBitmap`) and analytics. |
| Generate raster planes (visual/priority/control) during decode. | ✅ Done | `PicRasterizer` produces planes; medium/long line & pattern paths validated via unit tests. |

| Exit Criteria | Status | Notes |
|---------------|--------|-------|
| Template PICs decode without errors; state snapshots validate against legacy data. | ✅ Done | `PicPhaseTwoValidationTests.TemplatePicsDecodeWithoutErrors` parses all TemplateGame PICs; snapshot tests lock final state for SCI0/SCI1.1 exemplars. |
| Opcode histogram matches legacy tool output for sampled PICs. | ✅ Done | Snapshot tests assert opcode histograms for representative TemplateGame PICs (`PicPhaseTwoValidationTests`). |

**Outcome:** Phase 2 decoder criteria satisfied. Remaining analytics (palette usage/draw order) move under Phase 3 tooling work.

### Phase 3 – Rendering & Analytics

| Objective | Status | Notes |
|-----------|--------|-------|
| Finalize `PicDocument` model (commands + planes + metadata). | ✅ Done | Document encapsulates commands, planes, state timeline, and metadata wiring via `PicResourceCodec`. |
| Implement renderer/exporter creating PNG/BMP outputs for visual/priority/control. | ✅ Done | CLI export (`--export`) produces palette-aware PNG/BMP with legends for priority/control planes. |
| Add CLI tooling to visualize and inspect decoded scenes. | ⚠️ In Progress | CLI now supports summaries (`--pic-summary`) covering opcode/plane/palette analytics (including draw-order/plane state) and pixel comparisons (`--compare`, `--compare-baseline`). Opcode stream dumps now available via `--pic-ops`; re-encode diff tooling remains. |

| Exit Criteria | Status | Notes |
|---------------|--------|-------|
| Visual comparison (pixel diff within tolerance) between rendered outputs and legacy renders for SCI0/SCI1.1 templates. | ⚠️ In Progress | Baseline PNGs captured under `Companion.Maui/tests/Baselines/Pic` (e.g., `pic_001_visual.png`); `--compare`/`--compare-baseline` enable diff automation. CI wiring still pending. |
| CLI can export planes and show state info for arbitrary PICs. | ✅ Done | `--export`, `--pic-summary`, and inspection paths cover arbitrary PIC resources. |

**Next Steps:** integrate baseline comparisons into CI reporting and add re-encode diff tooling before closing Phase 3.

### Phase 4 – Encoder & Round-Trip

| Objective | Status | Notes |
|-----------|--------|-------|
| Serialize `PicDocument` back into opcode stream preserving semantics. | ⚠️ In Progress | `PicEncoder` scaffold returns original payloads; opcode dump tooling in CLI exposes current streams for analysis. |
| Reapply compression and ensure byte-identical round-trip where edits are neutral. | ⚠️ Pending | Compression encoder not yet implemented; existing path reuses original compressed body. |
| Provide diagnostics for unsupported edits. | ⚠️ Pending | To be addressed once encode path materialises. |

| Exit Criteria | Status | Notes |
|---------------|--------|-------|
| Template PICs round-trip byte-for-byte through decoder→encoder pipeline. | ⚠️ Pending | `PicEncoder` currently pass-through via original payload; full encode work still required. |
| Regression tests catch deviations and report actionable errors. | ⚠️ Pending | Baseline comparison tests in `PicBaselineComparisonTests` in place; encoder-specific regressions to follow. |
