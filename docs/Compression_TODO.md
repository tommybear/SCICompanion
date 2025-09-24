# Compression TODO

## Algorithm Implementation
- [x] Raw passthrough service (verify API consistency).
- [x] LZW (SCI0) decoder with 9–12 bit token handling.
- [x] LZW_1 decoder (SCI1) covering bit-width resets and token table growth.
- [x] ReorderPic transformation post LZW_1 (SCI1 PIC specific).
- [x] ReorderView transformation (for views; ensures compatibility).
- [x] DCL decoder for SCI1.x (methods 18–20).
- [x] STACpack decoder (method 32) – implementation landed; add fixtures before enabling by default.
- [ ] Optional encoders (LZW/DCL) if round-trip compression is required later.

## Registry & DI
- [x] Register method→service mappings (0,1,2,3,4,8,18–20,32) across DI and tests.
- [ ] Provide fallback diagnostics for unsupported methods.
- [x] Ensure DI injects registry into PIC/View codecs.

## Testing
- [x] Capture compressed/decompressed fixtures from template PIC resources (SCI0/SCI1.1).
- [x] Capture additional fixtures for VIEW resources (SCI1.1 method 19).
- [x] Write unit tests per algorithm verifying output matches fixtures (PIC + DCL coverage in `CompressionFixtureTests`).
- [x] Capture additional fixtures for SOUND resources (SCI0 sound `#2` baseline).
- [x] Add STACpack fixture exercising decoder copy semantics (synthetic payload until SCI2 assets land).
- [x] Add failure tests (unexpected method, corrupted token stream).
- [ ] Optional property tests for LZW to cover random token sequences.

## Tooling
- [x] CLI command to dump compression metadata (method name, size before/after) via `Companion.Cli --compression`.
- [ ] Debug helper to decompress resource via CLI for manual inspection.

## Documentation
- [ ] Document algorithm implementations, references (SCI Companion, ScummVM, FreeSCI).
- [ ] Outline extension points for additional compression methods.
