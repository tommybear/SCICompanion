# Compression TODO

## Algorithm Implementation
- [x] Raw passthrough service (verify API consistency).
- [x] LZW (SCI0) decoder with 9–12 bit token handling.
- [x] LZW_1 decoder (SCI1) covering bit-width resets and token table growth.
- [x] ReorderPic transformation post LZW_1 (SCI1 PIC specific).
- [x] ReorderView transformation (for views; ensures compatibility).
- [x] DCL decoder for SCI1.x (methods 18–20).
- [ ] STACpack decoder (method 32) – plan for future support.
- [ ] Optional encoders (LZW/DCL) if round-trip compression is required later.

## Registry & DI
- [x] Register method→service mappings (0,1,2,3,4,8,18–20,32) across DI and tests.
- [ ] Provide fallback diagnostics for unsupported methods.
- [ ] Ensure DI injects registry into PIC/View codecs.

## Testing
- [x] Capture compressed/decompressed fixtures from template PIC resources (SCI0/SCI1.1).
- [ ] Capture additional fixtures for VIEW/SOUND resources as needed.
- [x] Write unit tests per algorithm verifying output matches fixtures (PIC coverage in `CompressionFixtureTests`).
- [ ] Add failure tests (unexpected method, corrupted token stream).
- [ ] Optional property tests for LZW to cover random token sequences.

## Tooling
- [ ] CLI command to dump compression metadata (method name, size before/after).
- [ ] Debug helper to decompress resource via CLI for manual inspection.

## Documentation
- [ ] Document algorithm implementations, references (SCI Companion, ScummVM, FreeSCI).
- [ ] Outline extension points for additional compression methods.
