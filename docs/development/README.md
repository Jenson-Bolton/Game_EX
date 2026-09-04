# Working agreement and release workflow

This is the operating agreement for Game_EX. It keeps discussion, implementation, evidence, and published history aligned while the project is still changing quickly.

## Working principles

- Deliver the smallest complete vertical slice that answers the agreed question.
- Separate reusable engine code, offline world compilation, runtime world data, game rules, and editor-only workflows according to the documented dependency boundaries.
- Prefer measured evidence over assumptions. Keep test inputs small, deterministic, and representative.
- Preserve the distinction between source data, derived data, and project-authored content.
- Do not add speculative modules, generic managers, or abstractions with no current caller.
- Use “Czech Republic” in project prose and attribution.

## Specification gate

Routine implementation may proceed when it stays within an accepted scope and existing decisions. Pause and ask the project owner before code commits the project to any of the following:

- a new subsystem, executable, repository, or cross-project dependency;
- a third-party library, service, or tool that becomes part of the supported workflow;
- a persistent file format, compatibility promise, or destructive data migration;
- a rendering API, RHI boundary, editor UI framework, threading model, or long-lived resource model;
- a geographic extent, coordinate reference system, unit convention, data source, semantic classification, or rule for combining world layers;
- licensing terms, redistribution conditions, or generated assets whose provenance is unclear;
- an irreversible operation or a material expansion beyond the agreed version.

At the gate, state the decision required, the evidence already available, credible alternatives, and the consequences of each option. Record an accepted architectural choice as an ADR. Update project or section documentation when the decision concerns behaviour rather than architecture.

## Version unit

Game_EX uses Semantic Versioning. Before a stable `1.0.0` contract:

- a patch version (`0.1.1`) is a compatible, bounded refinement or independently verifiable implementation slice;
- a minor version (`0.2.0`) marks a meaningful integrated capability and reports how its preceding patch slices fit together;
- a major version is reserved for the future stable compatibility contract.

Every published version is one coherent release unit containing:

1. the agreed implementation and no unrelated work;
2. updated tests and verification evidence;
3. complete Doxygen comments for changed C++ code;
4. updated human documentation and data provenance;
5. one changelog entry;
6. one technical report in `docs/history/reports`;
7. one version increment propagated from the central version definition;
8. one release commit, one annotated `vMAJOR.MINOR.PATCH` tag, and one publication of the branch and tag to GitHub.

If local work requires several temporary commits, consolidate it into a coherent release state before publication. Do not publish the implementation and add its report or provenance in a later version. The version tag is the canonical immutable reference; the report records the exact release reference and its baseline.

The `v0.1.0` foundation predates this agreement. Its report is therefore a declared bootstrap exception and is anchored to its original commit rather than rewritten history. All versions from `v0.1.1` onward follow this workflow.

## Change workflow

1. **Frame the slice.** Write its aim, accepted baseline, in-scope deliverables, exclusions, and acceptance evidence.
2. **Resolve specification gates.** Ask for decisions that cannot be derived safely, then add or update an ADR when required.
3. **Choose the version.** Reserve the next SemVer value and add a draft report from the template.
4. **Implement narrowly.** Keep dependency direction and ownership explicit. Update documentation alongside behaviour.
5. **Verify.** Build, run focused tests and the complete relevant suite, generate strict Doxygen, and inspect any visual result that forms part of acceptance.
6. **Review provenance.** Record the source, licence, attribution, geographic/temporal coverage, coordinate reference system, transformations, and redistribution status of every data or asset input.
7. **Close the report.** Replace plans with observed results, limitations, reflection, and the recommended next slice.
8. **Publish once.** Update the central version, changelog, history index, and report; review the diff; create the release commit and annotated tag; push both to GitHub.

Failed or partial work stays unreleased. Record useful investigation in the next completed report or an ADR, but do not tag a version whose acceptance checks failed.

## Required verification

Run at least the workspace checks for a normal C++ version:

```powershell
cmake --preset vs2022
cmake --build --preset debug
ctest --preset debug
cmake --build --preset docs
```

Add focused tests for the changed behaviour. For GUI work, perform the automated window smoke test and a short visual inspection. For compiler or world-data work, verify malformed-input handling, deterministic output, coordinate and unit invariants, and at least one known real-data sample. Reports must distinguish command output from visual observation and must not claim evidence that was not captured.

## Doxygen and human documentation

Doxygen is part of the build contract, not a release afterthought:

- every C++ source and header has an `@file` description;
- public types and functions document purpose, parameters, return values, errors, ownership, lifetime, units, and invariants as applicable;
- non-obvious private algorithms and state constraints are documented where they are implemented;
- overrides reuse unchanged contracts with `@copydoc`;
- warnings, undocumented symbols, invalid parameter names, and broken references fail the `docs` target.

The generated API reference under the build tree is the authoritative code-level reference and is never committed. Markdown under `docs` describes why the system exists, its boundaries, workflow, evidence, and results. It links to the generated reference where needed instead of copying class or function documentation that can drift from the code.

## Data and asset provenance

Before committing a data file or derived asset, record:

- provider, dataset/product name, original location, and acquisition date;
- licence, required attribution, redistribution constraints, and any share-alike obligation;
- whether the file is raw, transformed, sampled, generated, or project-authored;
- geographic bounds, date or epoch, coordinate reference system, axis order, units, resolution, and no-data convention where applicable;
- the exact transformation tool, command, configuration, and input/output checksums needed to reproduce it;
- why the committed sample is sufficient and why any larger source file is excluded.

Do not commit data while its licence or provenance is unresolved. A compiler must not silently combine layers that use incompatible coordinate systems, units, epochs, or semantic assumptions.

## Definition of done

A version is done only when all applicable statements are true:

- scope and specification decisions are agreed and reflected in documentation;
- code builds warning-clean and relevant automated tests pass;
- user-visible or visual behaviour has been inspected where applicable;
- Doxygen completes with warnings treated as errors and covers every changed C++ file;
- human documentation is current without duplicating generated API documentation;
- data and assets have reproducible provenance and compliant attribution;
- the report states observed evidence, limitations, and next work honestly;
- the central version, changelog, history index, report, release commit, annotated tag, and pushed GitHub refs all identify the same release.
