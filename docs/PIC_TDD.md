# PIC Subsystem – Clean-Room Rewrite TDD Plan

## 1. Scope and Goals
- Provide a feature-complete, cross-platform implementation of Sierra SCI PIC resource handling for SCI0 through SCI1.1.
- Support decoding, in-memory representation, editing, and round-trip encoding of visual, priority, and control planes (including palette state, pattern fills, and polygons).
- Integrate lossless compression/decompression for PIC payloads (raw, LZW_Pic, LZW/Sierra variants, DCL for SCI1.x).
- Surface deterministic metadata for rendering, analytics (opcode counts), and future editors.

## 2. Architectural Overview
```
+-----------------------------+
| CompressionRegistry         |
|  - ICompressionService[]    |
|  - Decompress(data, method) |
+--------------|--------------+
               |
+--------------v--------------+
| PicDecoder                   |
|  - Decompress payload        |
|  - Parse opcodes             |
|  - Maintain state (palette,  |
|    pens, priority/control)   |
|  - Produce PicDocument       |
+--------------|--------------+
               |
+--------------v--------------+
| PicDocument                  |
|  - Planes: Visual/Control/   |
|    Priority bitmaps          |
|  - Palette history           |
|  - Command list              |
|  - Analytics (opcode counts) |
+--------------|--------------+
               |
+--------------v--------------+
| PicEncoder                   |
|  - Serialize commands        |
|  - Apply compression         |
|  - Validate byte identity    |
+-----------------------------+
```

## 3. Modules & Responsibilities
### 3.1 Compression Layer
- Implement SCI-specific algorithms: raw (0), LZW, LZW1, LZW_Pic, DCL, STACpack.
- Provide streaming-friendly APIs to minimize intermediate allocations.
- Unit-test against captured payloads from legacy assets (fixtures) and cross-compare with SCI Companion’s decompressed output.

### 3.2 Parser & Interpreter
- Opcode coverage for SCI0/SCI1.x datasets (set color, line draws, fills, patterns, polygons, XOR, pen toggles, extended opcodes).
- Manage palette banks (40-color, SCI1 VGA palettes), pattern tables, and composite state (priority lines, control zones).
- Produce plane buffers (visual/priority/control) using deterministic rasterization.
- Track command metadata (counts, bounding boxes, palette usage) for analytics.

### 3.3 Renderer Representation
- `PicDocument` encapsulating:
  - Command list (immutable).
  - Raster planes (bitmaps or vector primitives).
  - Palette timeline and priority band definitions.
- Provide adapter to render via MAUI/Skia and to export to PNG/BMP.

### 3.4 Encoder
- Convert `PicDocument` back to command stream preserving fluency (order, relative coordinates, pattern state).
- Reapply compression matching original method and verify byte identity or supported deviations (documented).
- Provide diagnostics for irreversible edits (e.g., operations outside supported subset).

## 4. Testing Strategy
### 4.1 Unit Tests
- Compression round-trips with golden data for each algorithm.
- Opcode parsing for synthetic command sequences (edge cases: nested polygons, fill boundaries, pattern toggles).
- Palette state changes (SCI0 vs SCI1.1).

### 4.2 Integration Tests
- Load official template PICs (SCI0, SCI1.1), decode to `PicDocument`, render to bitmap, compare against known outputs (pixel diff with tolerance for palette rounding).
- Re-encode and compare raw bytes to original resources (allowing reordering only if lossless editing not possible).
- CLI/Tooling smoke tests: decode view, export image, display metadata.

### 4.3 Property-Based Tests
- Generate random command sequences within supported opcode set to ensure parser/encoder stability and round-trip invariants.

### 4.4 Performance & Regression
- Benchmark decode/encode of large scenes, track memory allocation profiles.
- Regression tests capturing bug fixes (e.g., off-by-one fills, palette lock handling).

## 5. Tooling & Fixtures
- Reuse SCI template assets under `TemplateGame/SCI0` and `SCI1.1`.
- Add curated fixtures (decompressed outputs) for compression testing.
- Provide CLI commands to dump intermediate state for manual inspection.
- Verify palette-aware CLI exports (PNG/BMP) against legacy renders; include legend validation for priority/control planes.
- Use `--pic-summary`, `--compare`, `--compare-baseline`, and `--pic-ops` for opcode/plane analytics, pixel-diff validation, and command stream inspection when capturing regression evidence.
- Baseline comparison tests (`PicBaselineComparisonTests`) run in CI to ensure renderer output matches stored PNGs for selected SCI0/SCI1.1 PICs.

## 6. Risks & Mitigations
- **Compression parity**: Validate algorithm correctness against SCI Companion outputs; consider leveraging existing open-source implementations (ScummVM) if license-compatible.
- **Rendering semantics**: Differences in priority/control handling can cause regressions; extensively compare with legacy tool renders.
- **Command re-encoding**: Some legacy data relies on specific command sequences; document unsupported edits and provide fallbacks.

## 7. Deliverables Summary
- Managed compression library with tests.
- Comprehensive PIC decoder/encoder + domain model.
- Rendering adapters and CLI utilities.
- Golden tests ensuring compatibility with Sierra assets.
