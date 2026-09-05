# ADR 0009: Portable terrain stage and world package

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

The restarted Game_EX needs a concrete boundary between offline world preparation and runtime consumption before real Czech terrain, authoritative vector data, or TESSERA-derived semantics are introduced. The previous `WorldHeader` proved only a logical dependency direction; it did not define serialised bytes, a reproducible staging contract, or a reader that the game and world editor could safely consume.

The first contract must be small enough to verify without choosing the production geospatial toolchain. It must also preserve the architectural direction established by [ADR 0005](0005-world-compiler-repository-split-gate.md): specialised acquisition, reprojection, geospatial, network, and machine-learning dependencies stay offline and must not leak into Engine or Game. Production source selection and redistribution rights are separate concerns from proving the portable package boundary.

Terrain coordinates in the Czech proof region are large, negative EPSG:5514 values. Storing every world-space component as a float would lose useful precision, while storing only absolute heights would make tile-local processing less explicit. The pipeline also needs to distinguish declared source properties from the normalised stage and retain enough provenance to audit a derived package.

## Decision

Game_EX release `v0.1.4` introduces a deliberately bounded, standard-library-only C++20 terrain slice. It remains in the monorepository for now and consists of three build responsibilities:

- `GameEX::WorldFormat` owns the public logical types and the validated runtime reader `game_ex::world_format::read_world_package`;
- `GameEX::WorldCompilerCore` owns strict staging validation and deterministic encoding through `game_ex::world_compiler::load_staging_world` and `game_ex::world_compiler::compile_world`;
- `game_ex_world_compiler` exposes those offline operations through a command-line interface.

Game and the world editor may depend on `GameEX::WorldFormat`. They must not link `GameEX::WorldCompilerCore`, invoke acquisition code as part of runtime loading, or depend on the compiler executable. The internal encoder is not a public runtime API.

The accepted stage is a strict `game_ex_staging_manifest` version 1 file, conventionally named `manifest.gexstage`, plus one text height file and one text validity-mask file. The schema requires exactly one terrain layer and one terrain tile. Every required key is present exactly once, unknown keys are rejected, referenced files are confined beneath the resolved manifest directory, and their declared byte sizes and SHA-256 digests are verified before samples are parsed. Equivalent validated inputs are encoded in a fixed order without host-structure dumps, compression, build timestamps, random identifiers, or filesystem iteration.

The accepted runtime result is `.gexworld` format version `0.2`. It uses an explicit 128-byte little-endian header, row-major IEEE-754 binary32 local-height offsets, one canonical validity byte per sample, and a fixed-order provenance block. CRC-32/ISO-HDLC covers the complete file while treating the stored checksum field as zero. Section offsets are canonical, all allocations and strings are bounded, and trailing or extension bytes are rejected. The [world-package format specification](../architecture/world-package-format.md) is normative for both the stage and wire representation.

The coordinate contract is:

- horizontal coordinates are EPSG:5514 and use double-precision `origin_x`, `origin_y`, `spacing_x`, and `spacing_y`;
- columns advance in positive X, rows advance in positive Y, and `index = row * columns + column`;
- `origin_z` is an absolute double-precision elevation in the declared stage vertical reference;
- each stored height is a finite float offset from `origin_z`, so consumers reconstruct a valid sample as `origin_z + double(heights[index])`;
- no-data is represented independently by a zero-or-one validity mask, so an invalid cell still has a finite stored height but must not contribute to valid terrain statistics or surfaces;
- numeric runtime bounds are derived from the origin, spacing, and dimensions. The human-readable `spatial.bounds` field is provenance and is not trusted as geometry.

Provenance records the original source identity, acquisition time, licence and attribution, asserted original byte size and SHA-256, source CRS/axis/units/vertical/no-data properties, the corresponding normalised-stage properties, resolution, declared bounds, epoch, ordered transformation record, responsible owner, and verified staged-file paths/sizes/digests. The portable compiler verifies the two staged files. It preserves, but cannot independently re-acquire or recompute, the original-source size and digest because this milestone intentionally has no source adapter.

The repository split gate from ADR 0005 remains binding. Before a supported workflow introduces GDAL, PROJ, Python or a Python environment, network acquisition, credentials, TESSERA access/decoding/training, or another machine-learning dependency, development stops for the repository decision. The default remains to split WorldCompiler into its own history-preserving repository and publish `GameEX::WorldFormat` as a versioned package. A superseding ADR is required to retain such dependencies in this monorepository.

The committed `WorldCompiler/tests/fixtures/minimal` inputs are project-authored synthetic test data. They contain negative EPSG:5514-style coordinates, a non-zero Z origin, four small height/mask samples, and explicit project provenance solely to exercise the contract. They are not DMR 5G, TESSERA, or another real-world dataset; they do not establish a production source adapter or external redistribution permission. Loading this package in the game or world editor is also outside this decision.

## Consequences

The compiler and runtime now share a concrete, portable contract that can be built and tested independently with C++20 and the standard library. Identical accepted inputs produce identical package bytes, provenance travels with the runtime package, corrupt or structurally ambiguous files fail closed, and large EPSG:5514 coordinates do not have to be reduced to floats.

This slice is intentionally not a general geospatial compiler. It supports only EPSG:5514, one layer, one tile, at most 4096 samples along either dimension, at most 4,194,304 total samples, and at most a 64 MiB package. Staging uses human-auditable text rather than a high-throughput interchange format. The package has no vector layers, material probabilities, multiple tiles, spatial index, compression, streaming table, or renderer resources. Any of those additions require a versioned format decision rather than reinterpretation of reserved bytes.

Exact minor-version decoding means a future layout cannot silently pass because its major version matches. `game_ex::world_format::is_supported` remains only a lightweight logical-header check; untrusted files must pass through `read_world_package`, which currently accepts exactly format `0.2`.

The original-source hash is an auditable assertion at this portable boundary, not proof that the source is currently available. A future source adapter must acquire and hash the original itself before producing a manifest. Licence review, source terms, acquisition evidence, and raw-data retention remain required for real data.

## Alternatives considered

Splitting WorldCompiler immediately would create remote naming, package publication, CI, and compatibility policy before the portable boundary has implementation evidence. Keeping all future tooling in the monorepository would expose Game and Engine to specialised dependency, licence, storage, and release concerns, so the existing split gate is retained.

Using JSON, YAML, a GIS container, or a third-party serialisation library would be more extensible but would introduce a parser or dependency before the first contract needs it. A strict line-oriented manifest keeps the milestone reviewable; it is not a commitment that production acquisition inputs must remain text.

Dumping native C++ structures would make padding, ABI, alignment, endianness, and compiler choices part of the file format. The explicit wire encoding avoids those dependencies. Storing absolute world coordinates and elevations entirely as floats would be simpler but unsuitable for the intended EPSG:5514 scale. Storing no-data as a magic height would conflate absence with measurement and make valid extrema ambiguous.

Loading LAS/LAZ, provider feeds, or TESSERA directly in the editor would shorten the first demonstration path, but it would move heavy offline policy and untrusted-source processing into an interactive runtime. Omitting provenance would make deterministic bytes easier while making the resulting package impossible to audit responsibly; provenance is therefore mandatory even in the synthetic fixture.
