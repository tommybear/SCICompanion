# Compression TODO

## Algorithm Implementation
- [ ] Raw passthrough service (completed, verify API consistency).
- [ ] LZW (SCI0) decoder with 9–12 bit token handling.
- [ ] LZW_1 decoder (SCI1) covering bit-width resets and token table growth.
- [ ] ReorderPic transformation post LZW_1 (SCI1 PIC specific).
- [ ] ReorderView transformation (for views; ensures compatibility).
- [ ] DCL decoder for SCI1.x (methods 18–20).
- [ ] STACpack decoder (method 32) – plan for future support.
- [ ] Optional encoders (LZW/DCL) if round-trip compression is required later.

## Registry & DI
- [ ] Register method→service mappings (0,1,2,4,18–20,32).
- [ ] Provide fallback diagnostics for unsupported methods.
- [ ] Ensure DI injects registry into PIC/View codecs.

## Testing
- [ ] Capture compressed/decompressed fixtures from template resources (SCI0/SCI1.1).
- [ ] Write unit tests per algorithm verifying output matches fixtures.
- [ ] Add failure tests (unexpected method, corrupted token stream).
- [ ] Optional property tests for LZW to cover random token sequences.

## Tooling
- [ ] CLI command to dump compression metadata (method name, size before/after).
- [ ] Debug helper to decompress resource via CLI for manual inspection.

## Documentation
- [ ] Document algorithm implementations, references (SCI Companion, ScummVM, FreeSCI).
- [ ] Outline extension points for additional compression methods.
