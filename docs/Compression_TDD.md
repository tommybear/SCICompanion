# PIC Compression Subsystem – TDD Plan

## 1. Goals
- Provide managed implementations for all SCI compression schemes needed by PIC resources (raw, LZW, LZW_1, LZW_Pic reorder, DCL, STACpack).
- Integrate algorithms into `CompressionRegistry` with a consistent streaming-friendly API.
- Guarantee byte-accurate decompression across SCI0–SCI1.1 sample assets and enable future re-encoding.

## 2. Architecture
```
+------------------------------+
| ICompressionService          |
|  - SupportsMethod(method)    |
|  - Decompress(data, method)  |
|  - (Later) Compress          |
+------------------------------+
            ^        ^
            |        |
+-----------|--------|-----------+
| LzwCompressionService          |
|  - LZW / LZW_1 decoding        |
|  - optional Compressor         |
+--------------------------------+

+--------------------------------+
| PicReorderService (post-LZW)   |
|  - Applies reorderPic          |
+--------------------------------+

+--------------------------------+
| DclCompressionService          |
|  - Implements SCI1 DCL         |
+--------------------------------+

+--------------------------------+
| StacCompressionService         |
|  - Implements STACpack         |
+--------------------------------+

+------------------------------+
| CompressionRegistry           |
|  - Method→Service map         |
|  - Fallback handling          |
+------------------------------+
```

## 3. Requirements
### 3.1 Algorithms
- **Raw (0)**: Passthrough for completeness.
- **LZW (method 1)**: SCI0 variant (Carl Muckenhoupt’s algorithm) with 9–12 bit tokens.
- **LZW_1 (method 2)**: SCI1 variation used for views and PIC; tied to reorderPic/reorderView post-processing. *Status:* decoder implemented (2025-09-23) with synthetic regression tests; fixture comparison still pending.
- **LZW_Pic (method 4)**: LZW_1 + reorderPic transformation. *Status:* reorder pipeline implemented (2025-09-23); golden fixtures pending DCL decode support for SCI1.x buffers.
- **DCL (methods 18–20)**: SCI1.x decompression used for PIC and other resources. *Status:* managed decoder implemented (2025-09-23); extend fixtures/CLI reporting.
- **STACpack (method 32)**: For later SCI2 resources (optional but plan for extension).

### 3.2 Implementation Detail
- Streaming decode APIs to avoid large temporary buffers where possible.
- Clear error handling (unknown tokens, buffer overflow); throw descriptive exceptions.
- Support writing output into preallocated buffers and returning actual decompressed length.

## 4. Testing Strategy
### 4.1 Fixtures
- Extract compressed payloads from template games (SCI0, SCI1.1) and reference decompressed buffers via legacy tool (SCI Companion).
- Store fixtures under `tests/Fixtures/pic/compression/` with both compressed and expected decompressed bytes.

### 4.2 Unit Tests
- Method-specific decode tests comparing output to golden data.
- Negative tests for unsupported methods and malformed streams.
- Optional property tests generating short synthetic sequences (for LZW token tables).

### 4.3 Integration Tests
- End-to-end decompress → reorder (if needed) → decode PIC commands to ensure compatibility with downstream parser.

### 4.4 Performance
- Benchmark decode throughput (compressed size → decompress time) on representative payloads.
- Monitor allocations; ensure streaming APIs minimize copying.

## 5. Risks & Mitigations
- **Algorithm correctness**: Validate against multiple fixtures; cross-reference with SCI Companion/ScummVM implementations.
- **Performance**: Use efficient bit readers and reusable buffers; benchmark early.
- **Maintainability**: Document algorithm steps, especially for LZW variants and DCL bit packing.

## 6. Deliverables
- Managed compression services + registry wiring.
- Comprehensive tests & fixtures.
- Developer documentation describing algorithms, method IDs, and usage.
