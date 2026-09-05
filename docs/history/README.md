# Version history and technical reports

Each published Game_EX version has a report that connects its aim, implementation, evidence, data provenance, limitations, and next work. The changelog is a concise list of externally meaningful changes; these reports are the fuller technical record.

## Chronological index

| Version | Date | Status | Report | Release reference | Summary |
| --- | --- | --- | --- | --- | --- |
| `v0.1.0` | 2026-09-04 | Foundation baseline | [Report](reports/v0.1.0.md) | Retrospective [`v0.1.0` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.0) on commit `2ac180d2064c1a7e57e4631259df2a3884b57998` | Three-project C++20 workspace, SDL3 game/editor windows, logical world header, tests, and strict Doxygen. |
| `v0.1.1` | 2026-09-05 | Released | [Report](reports/v0.1.1.md) | Annotated [`v0.1.1` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.1) | Working agreement, versioned evidence history, and central version propagation. |
| `v0.1.2` | 2026-09-05 | Released | [Report](reports/v0.1.2.md) | Annotated [`v0.1.2` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.2) | Validated deterministic serial startup, rollback, application integration, and recorded real-data/compiler boundaries. |
| `v0.1.3` | 2026-09-05 | Released | [Report](reports/v0.1.3.md) | Annotated [`v0.1.3` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.3) | Bounded worker batches and deterministic controlled-parallel startup with explicit affinity and failure barriers. |
| `v0.1.4` | 2026-09-05 | Released | [Report](reports/v0.1.4.md) | Annotated [`v0.1.4` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.4) | Strict portable terrain stage, deterministic package, runtime reader, provenance, integrity, and CLI inspection. |
| `v0.1.5` | 2026-09-05 | Released | [Report](reports/v0.1.5.md) | Annotated [`v0.1.5` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.5) | Shared diagnostic Render API, OpenGL 4.6 Core backend, application lifecycle integration, and real game/editor presentation. |
| `v0.1.6` | 2026-09-05 | Released | [Report](reports/v0.1.6.md) | Annotated [`v0.1.6` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.6) | Vulkan 1.3 transfer-clear backend, strict renderer selection, semantic cross-technology parity, and validation-backed presentation. |
| `v0.2.0` | Planned | Planned roll-up | Report created with the release | — | First integrated world-data/compiler/editor capability, summarising its independently verified patch slices. |

## Bootstrap exception

The repository's 52 earlier commits remain preserved in Git and culminate in pre-restart commit [`43e9bff4026f1d2f1d3916ac130403af05ba96cf`](https://github.com/Jenson-Bolton/Game_EX/commit/43e9bff4026f1d2f1d3916ac130403af05ba96cf). They predate the new orientation and per-version reporting system, so this report sequence begins with the restart rather than inventing reports that did not exist for the earlier work.

The foundation commit was published before this reporting workflow existed. The `v0.1.0` report is retrospective and was added during `v0.1.1`; it is anchored to the immutable foundation commit shown above. The annotated `v0.1.0` tag was applied to that existing commit on 2026-09-05. No history was rewritten, and the report does not claim that the tag or report accompanied the original push.

From `v0.1.1` onward, the implementation, report, changelog, version increment, annotated tag, and GitHub publication form one release unit.

Use the [technical report template](report-template.md) and the [working agreement](../development/README.md) for every new version.

## Recovered pre-restart evidence

The [legacy Bystřice data and provenance audit](legacy-bystrice-data-audit.md) records the exact coursework terrain/OSM artefacts, conversion behaviour, and evidence gaps without importing them. Its current acceptance decision is deliberately negative: those files may inform external diagnostics but are not accepted, copyable, redistributable, or combinable Game_EX data.
