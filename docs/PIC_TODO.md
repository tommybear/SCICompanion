# PIC Implementation TODO

## Compression Layer
- [x] Implement SCI LZW (method 0x0000) decoder in managed code with streaming API.
- [x] Implement SCI LZW_1 variant (method 0x0002).
- [x] Implement LZW_Pic post-processing (reorderPic) after LZW_1 decode.
- [x] Implement DCL decompression (methods 0x0012-0x0014, 0x0014 used for PIC in SCI1.1) so LZW_Pic reorder has full buffers to operate on.
- [x] Implement STACpack (method 0x0020); decoder available pending fixture validation.
- [x] Add compression registry wiring for all algorithms (method → service mapping).
- [ ] Create golden fixture tests per algorithm (compare against legacy decompressed buffers).

## Parser & State Engine
- [x] Port opcode enumerations (SCI0/SCI1.x) to managed constants (set color, pattern, lines, fill, polygons, XOR, extended functions).
- [x] Implement state machine maintaining:
  - [x] Palette banks (40-color, VGA palette, locks).
  - [x] Priority/Control enable flags and values.
  - [x] Pattern size/number codes and rectangle flags.
  - [x] Pen position for relative commands.
- [ ] Parse absolute/relative pattern commands with SCI0/SCI1 differences.
- [ ] Parse line commands (short/medium/long) with correct delta handling.
- [ ] Implement flood fill operations to update plane buffers.
- [ ] Support extended opcodes (OPX) for SCI0 (palette, pattern) and SCI1 (bitmaps, palette changes, priority bands).
- [ ] Maintain command metadata (opcode counts, bounding boxes, plane dirty flags).

## Rendering & Document Model
- [x] Define `PicDocument` data structure capturing:
  - [x] Command list (immutable representation).
  - [x] Visual, priority, control plane bitmaps (byte arrays or raster surfaces).
  - [x] Palette timeline, priority bands, pattern state snapshots.
- [x] Implement rasterizer applying commands to planes using decoded state (initial support for color, priority/control, lines, fill).
- [ ] Provide exporter to produce raster images for verification (PNG/BMP via Skia/WriteableBitmap).
- [ ] Implement analytics utilities (opcode histogram, palette usage, draw order).

## Encoder
- [ ] Map `PicDocument` back to opcode stream preserving semantics.
- [ ] Reapply compression using correct algorithm; support raw fallback for edited resources.
- [ ] Validate byte-for-byte identity against original resources; flag deviations with diagnostics.

## Tooling & CLI
- [ ] Extend CLI to export visual/priority/control planes as images.
- [ ] Add command to dump opcode stream with interpreted state (e.g., textual diff).
- [ ] Provide option to re-encode and compare diff (success/fail summary).

## Testing & Benchmarks
- [ ] Golden tests for decode/encode round-trip on template PICs (SCI0, SCI1.1).
- [ ] Pixel comparison tests between rendered planes and legacy renders.
- [ ] Property-based tests generating synthetic command sequences.
- [ ] Performance benchmarks (decode/encode time, memory allocations) tracked in CI.

## Documentation
- [ ] Update developer docs describing PIC architecture, compression mapping, state machine.
- [ ] Provide migration notes from legacy C++ implementation (mapping of files/classes).
