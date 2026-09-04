# Version history and technical reports

Each published Game_EX version has a report that connects its aim, implementation, evidence, data provenance, limitations, and next work. The changelog is a concise list of externally meaningful changes; these reports are the fuller technical record.

## Chronological index

| Version | Date | Status | Report | Release reference | Summary |
| --- | --- | --- | --- | --- | --- |
| `v0.1.0` | 2026-09-04 | Foundation baseline | [Report](reports/v0.1.0.md) | Retrospective [`v0.1.0` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.0) on commit `2ac180d2064c1a7e57e4631259df2a3884b57998` | Three-project C++20 workspace, SDL3 game/editor windows, logical world header, tests, and strict Doxygen. |
| `v0.1.1` | 2026-09-05 | Released | [Report](reports/v0.1.1.md) | Annotated [`v0.1.1` tag](https://github.com/Jenson-Bolton/Game_EX/tree/v0.1.1) | Working agreement, versioned evidence history, and central version propagation. |
| `v0.2.0` | Planned | Planned roll-up | Report created with the release | — | First integrated world-data/compiler/editor capability, summarising its independently verified patch slices. |

## Bootstrap exception

The repository's 52 earlier commits remain preserved in Git and culminate in pre-restart commit [`43e9bff4026f1d2f1d3916ac130403af05ba96cf`](https://github.com/Jenson-Bolton/Game_EX/commit/43e9bff4026f1d2f1d3916ac130403af05ba96cf). They predate the new orientation and per-version reporting system, so this report sequence begins with the restart rather than inventing reports that did not exist for the earlier work.

The foundation commit was published before this reporting workflow existed. The `v0.1.0` report is retrospective and was added during `v0.1.1`; it is anchored to the immutable foundation commit shown above. The annotated `v0.1.0` tag was applied to that existing commit on 2026-09-05. No history was rewritten, and the report does not claim that the tag or report accompanied the original push.

From `v0.1.1` onward, the implementation, report, changelog, version increment, annotated tag, and GitHub publication form one release unit.

Use the [technical report template](report-template.md) and the [working agreement](../development/README.md) for every new version.
