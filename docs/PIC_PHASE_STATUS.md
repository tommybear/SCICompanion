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
| Add CLI tooling to visualize and inspect decoded scenes. | ⚠️ In Progress | CLI now supports summaries (`--pic-summary`) covering opcode/plane/palette analytics and pixel comparisons (`--compare`). Draw-order analytics and opcode stream dumps remain. |

| Exit Criteria | Status | Notes |
|---------------|--------|-------|
| Visual comparison (pixel diff within tolerance) between rendered outputs and legacy renders for SCI0/SCI1.1 templates. | ⚠️ In Progress | `--compare` enables manual diffing; automated baselines/tests still outstanding. |
| CLI can export planes and show state info for arbitrary PICs. | ✅ Done | `--export`, `--pic-summary`, and inspection paths cover arbitrary PIC resources. |

**Next Steps:** automate diff baselines, surface draw-order analytics, and extend CLI inspections (opcode stream dumps) before closing Phase 3.
