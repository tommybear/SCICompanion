# Compression Subsystem Milestones

## Milestone C1 – Core LZW Support (Weeks 0–2)
**Objectives**
- Implement raw, LZW, and LZW_1 decompressors.
- Integrate registry wiring and ensure PIC/View codecs use decoded payloads.
- Establish fixtures and unit tests for SCI0 examples.

**Exit Criteria**
- SCI0 PICs and views decompress successfully with new services.
- Tests confirm byte-equivalence with legacy outputs.

## Milestone C2 – PIC-Specific Reorders & SCI1 Extensions (Weeks 2–4)
**Objectives**
- Add reorderPic/reorderView transformations.
- Support SCI1 LZW_Pic via registry method mapping.
- Extend fixtures/tests to SCI1.1 assets.

**Exit Criteria**
- SCI1.1 PICs decompress to buffers matching legacy tool outputs.
- PIC parser consumes decoded data without fallback.

## Milestone C3 – Advanced Algorithms (Weeks 4–6)
**Objectives**
- Implement DCL decoder (methods 18–20) and integrate with registry.
- (Optional) STACpack support for future compatibility.
- Provide CLI diagnostics for compression metadata.

**Exit Criteria**
- SCI1.x resources using DCL decode correctly; tests cover success/failure paths.
- CLI can report compression method names and ratios.

## Milestone C4 – Validation & Documentation (Weeks 6–8)
**Objectives**
- Finalize fixture set and ensure all algorithms have golden tests.
- Add property/negative tests and benchmark harnesses.
- Publish documentation (TDD overview, developer guide).

**Exit Criteria**
- All compression methods have comprehensive unit coverage.
- Documentation explains method mapping and implementation details.
