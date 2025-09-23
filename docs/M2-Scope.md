# M2 Scope – Resource Serialization Core

## Objectives
- Establish the resource codec architecture capable of encoding/decoding SCI resources independent of UI concerns.
- Implement foundational codecs for picture (PIC), view/animation, palette, text/message, vocabulary, and sound resources using cross-platform data structures.
- Provide golden-file based regression tests that load template assets, reserialize them, and verify interpreter-compatible output.
- Introduce shared services for resource validation, compression/decompression strategy selection, and error reporting.
- Integrate the codec registry into a new resource repository abstraction consumed by higher-level services.

## Deliverables
1. **Codec Infrastructure**
   - `IResourceCodec` interface exposing `Deserialize`, `Serialize`, and `Validate` semantics.
   - Codec registry with dependency injection integration and lookup by `ResourceType`.
   - Compression providers (e.g., LZW, Huffman) abstracted behind service interfaces.
2. **Implemented Codecs (Initial Set)**
   - PIC (vector commands, priority/control layers, palette dependencies).
   - View/Animation (loops, cels, palette references, hit masks).
   - Palette (SCI0/Sci1.1 variants).
   - Text/Message tables with language entries.
   - Vocabulary (nouns, cases, synonyms per SCI0 format).
   - Sound (digital/MIDI headers) with stub compression support if needed.
3. **Resource Repository**
   - API to enumerate resources from the resource map and fetch decoded entities on demand.
   - Support for patch directories and package volumes (`resource.000`, `resource.map`, `resource.xxx`).
4. **Tests and Fixtures**
   - Golden resource fixtures signed off from `TemplateGame/SCI0` and `TemplateGame/SCI1.1`.
   - Round-trip tests ensuring decode→encode preserves binary identity where feasible.
   - Validation tests for corrupted headers and unsupported resource types.

## Acceptance Criteria
- For each supported resource type, decoding and re-encoding sample assets produces byte-identical output (or documented allowable differences) compared to originals.
- Resource repository enumerates and loads assets from template games without exceptions.
- Unit/integration tests cover error cases (e.g., unknown compression, malformed data) with meaningful diagnostics.
- Compression services abstract actual implementation details, enabling future replacement or optimization.

## Out-of-Scope
- Advanced editing/manipulation UI (reserved for later milestones).
- SCI2+ resource formats beyond initial SCI0/SCI1.1 coverage.
- Audio synthesis or lip-sync pipelines (handled later).

## Dependencies
- M1 resource discovery and metadata systems.
- Access to legacy resource documentation and existing sample assets.
- Potential reuse of existing native compression libraries (evaluate licensing).

## Risks
- Achieving byte-identical reserialization for complex resources (PIC command ordering, view palettes) may require iterative reverse-engineering.
- Compression algorithm parity (especially sound/MIDI) could introduce platform dependencies.
- Repository abstraction must manage memory footprint when loading large resource sets; plan for streaming patterns.

## Next Steps
- Define codec interface contracts and set up tests harness.
- Prioritize PIC codec implementation as first target, with golden files generated from template resources.
- Catalog compression schemes used in template games to plan dependencies.
