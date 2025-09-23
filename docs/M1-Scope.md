# M1 Scope – Core Project Services

## Objectives
- Introduce project metadata models capturing game identity, interpreter targets, and resource management settings.
- Persist metadata to a new `.sciproj` format (JSON/YAML) with deterministic round-trip behavior.
- Discover existing SCI resource maps (`resource.map` + volume files) and emit normalized descriptors the rewrite can consume.
- Port version detection heuristics to classify SCI0 vs SCI1.1 projects and flag interpreter profiles.
- Deliver a CLI smoke utility that loads a project and enumerates resources for early adopters.

## Deliverables
1. **Domain Models**
   - `ProjectMetadata`, `ResourceDescriptor`, `InterpreterProfile` records housed in `Companion.Domain`.
   - Validation layer ensuring mandatory fields and compatible interpreter settings.
2. **Persistence Layer**
   - Serializer for `.sciproj` (default JSON, optional YAML behind feature flag).
   - Watcher for resource folder changes emitting domain events.
3. **Resource Discovery Service**
   - Parser for `resource.map` formats (SCI0, SCI1.1) capable of handling patch folders.
   - Integration tests using `TemplateGame/SCI0` and `TemplateGame/SCI1.1` fixtures.
4. **Version Detection**
   - Heuristics leveraging map data, interpreter binaries, and resource signatures.
   - Unit tests covering representative cases and failure states.
5. **CLI Utility**
   - `Companion.Cli` console project invoking discovery and printing summary tables.
   - Exit codes indicating success vs validation failures.

## Acceptance Criteria
- Loading and saving `.sciproj` on both template games yields byte-identical files across runs.
- Resource discovery matches legacy SCI Companion counts for all resource types.
- Interpreter version detection agrees with legacy tool outcomes for SCI0 and SCI1.1 samples.
- CLI enumerates resources and interpreter info without throwing on provided fixtures.
- Automated tests (unit + integration) cover metadata serialization, discovery, and detection flows.

## Out-of-Scope
- GUI integration beyond wiring project services into DI.
- Real-time file system monitoring beyond simple polling (advanced watchers deferred).
- Support for SCI versions beyond SCI0–SCI1.1.

## Dependencies
- Existing sample assets documented in `docs/M0-Scope.md`.
- MAUI solution scaffolding from M0.
- NuGet packages for JSON/YAML serialization (System.Text.Json, optional YamlDotNet).

## Risks
- Divergence between legacy SCI map parsing and new implementation; mitigate with characterization tests.
- Ambiguity in version detection for hybrid fan projects; surface warnings instead of hard errors.
- CLI UX may evolve; treat current version as beta with clear messaging.

## Next Steps
- Draft detailed tasks in `TODO.md` under the Foundation section.
- Set up test fixture directories under `Companion.Maui/tests` mirroring template assets.
- Begin implementing domain models and serializers under `Companion.Domain`.
