# ADR 0005: WorldCompiler repository split gate

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton

## Context

WorldCompiler currently shares the Game_EX repository with Engine and Game. That arrangement preserves the existing history, keeps the foundation build simple, and lets the compiler/runtime boundary be designed before remote repositories and package contracts are fixed.

The portable compiler core can define source descriptors, provenance requirements, coordinate/unit invariants, deterministic validation, diagnostics, and small normalised fixtures using C++20 and the standard library. Production acquisition and geospatial processing will eventually require a materially different toolchain: native GDAL/PROJ dependencies, Python environments, network/data-provider adapters, and offline machine-learning work for sources such as TESSERA. Allowing those dependencies to enter the shared build first would make the later repository boundary depend on accidental implementation details.

## Decision

Keep WorldCompiler in the current monorepository while implementing and testing only the portable manifest/domain model, dependency-light compiler core, WorldFormat contract, deterministic validation, and small already-resolved fixtures. Game continues to depend only on `GameEX::WorldFormat`; it must not link the compiler executable, acquisition code, or geospatial/ML tooling.

Before any supported WorldCompiler path adds or requires GDAL, PROJ, a Python interpreter or package environment, network acquisition credentials/adapters, or machine-learning acquisition/decoding/training dependencies, stop at a repository-split decision. The default recommendation at that gate is to perform the split. Remaining in the monorepository requires a superseding ADR that demonstrates how the heavy toolchain remains isolated from Engine, Game, their consumers, and their release builds.

The same gate is triggered earlier if any of these conditions occurs:

- compiler and game/engine releases need independent cadences or compatibility policies;
- compiler CI requires operating-system images, licences, credentials, storage, or execution time inappropriate for normal engine/game validation;
- raw or derived data requires separate access control, retention, large-file storage, or legal review;
- compiler dependencies begin affecting root configure time, package resolution, deployment, or supported platforms;
- WorldFormat needs a separately versioned package boundary that cannot be expressed safely inside the current release;
- separate maintainers, issue ownership, or security boundaries become necessary.

No GDAL/PROJ link, Python bootstrap, ML environment, acquisition service, credential, or large source dataset is added “temporarily” before this gate. Experiments outside the supported repository build may inform the decision, but their outputs enter Game_EX only through documented, checksummed fixtures and provenance.

## Intended topology and migration

The intended result is three repositories corresponding to the existing responsibility boundaries:

```text
Engine repository
  reusable core, platform, Render API/RHI, and renderer backends

WorldCompiler repository
  acquisition adapters, offline compiler, portable compiler core,
  and the versioned/installable WorldFormat package

Game repository
  game and world-editor applications, simulation, and project-specific content
  depending on released Engine and WorldFormat packages
```

Exact remote names, visibility, package registry, and local multi-repository workspace mechanism remain owner decisions. The current `Jenson-Bolton/Game_EX` repository is expected to become the Game repository so its complete history remains available. Engine and WorldCompiler are extracted from clones of the same pre-split commit using a history-preserving subtree/path-filter operation rather than copied into history-free repositories.

The migration plan is:

1. Complete and tag the last coherent monorepository release; freeze dependency and format changes during extraction.
2. Record the source commit, directory-to-repository mapping, tool/version/commands, resulting head commits, and tag policy in a migration report.
3. Extract `Engine` and `WorldCompiler` histories into new remotes, retaining commits that affected each path and importing only explicitly assigned shared build/documentation files.
4. Verify file inventories, relevant historical commits, authorship, builds, tests, Doxygen, licences, and commit mappings before publishing the new repositories.
5. Keep the existing repository history intact. Remove extracted source directories from the Game repository only in a new migration commit; never rewrite its published branch to pretend they were absent.
6. Replace sibling `add_subdirectory` bridges with versioned package discovery. Pin compatible Engine and WorldFormat releases in Game and reproduce the three-project build in the chosen local workspace mechanism.
7. Apply new repository protections, CI, ownership, release reports, and annotated baseline tags before feature development resumes.

Submodules, package-manager references, or a lightweight workspace manifest may later compose local checkouts, but no fourth authoritative source repository is selected by this ADR.

## Consequences

Portable contracts and tests can advance without prematurely choosing remote names or burdening every Game/Engine build with geospatial tooling. The split gate is early enough to keep heavy acquisition and ML dependencies on the offline side of a physical repository boundary.

Production ingestion of formats that require GDAL/PROJ, supported Python acquisition, and TESSERA decoding cannot be added until the gate is resolved. This intentionally creates a visible pause rather than an invisible build dependency. History-preserving extraction and package publication add migration work, and coordinated changes to WorldFormat will require explicit compatibility and release management after the split.

## Alternatives considered

Splitting immediately would create repository names, package contracts, CI, permissions, and release policy before the portable compiler boundary has evidence. Keeping the monorepository indefinitely would allow specialised compiler dependencies and restricted data workflows to leak into unrelated builds. Adding heavy dependencies now and extracting later would make the split harder and weaken the evidence that the chosen boundary is architectural rather than accidental.
