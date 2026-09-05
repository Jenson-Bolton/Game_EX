# World stage and package format

This document is the normative data contract implemented for Game_EX project release `v0.1.4`. It specifies staging-manifest version 1 and `.gexworld` wire format `0.2`. Those are separate version domains: changing the Game_EX release number does not change readable package bytes, and changing the package layout requires an explicit world-format compatibility decision.

The first concrete on-disk package is format `0.2`. The earlier logical header version `0.1` was never a serialised package contract and is not readable as one.

The implementation boundary is:

```text
manifest.gexstage + height text + validity text
       |                              |
       | load_staging_world           | compile_world
       v                              v
logical WorldPackage             internal encoder
                                      |
                                      v
                                  .gexworld
                                      |
                                      | read_world_package
                                      v
                             logical WorldPackage
```

`GameEX::WorldFormat` contains the public logical model and runtime reader. `GameEX::WorldCompilerCore` contains the offline stage parser and compiler and depends on WorldFormat. A runtime consumer may link `GameEX::WorldFormat`; it must not link the compiler core or use the internal encoder in `package_encoding.hpp`.

## Current milestone boundary

The format currently represents exactly one terrain layer containing exactly one tile. It does not represent vector features, materials, imagery, TESSERA embeddings or probabilities, multiple tiles, level of detail, compression, a spatial index, or renderer-specific data.

`WorldCompiler/tests/fixtures/minimal` is a project-authored, 2-by-2 synthetic contract fixture. It uses negative EPSG:5514-style coordinates, a non-zero vertical origin, and four hand-written height/mask values. It is not real terrain and has no DMR 5G, TESSERA, or other external-source adapter behind it. Its original-source record is deliberately labelled synthetic, and its rights statement does not grant external reuse rights. The world editor and game do not yet load or visualise this package in `v0.1.4`.

## Staging manifest version 1 (`.gexstage`)

The conventional manifest name is `manifest.gexstage`; the parser does not require that filename or extension. The manifest describes already-normalised terrain. Acquisition, reprojection, gridding, source licence review, and source-data validation happen before this boundary.

### File and line grammar

A manifest is a regular file of at most 65,536 bytes. It is read as exact bytes without locale-dependent text translation. The complete file, including comments, must be valid shortest-form UTF-8. Control bytes below `0x20` are forbidden except line-feed and carriage-return; `0x7f` is also forbidden. A carriage-return is valid only immediately before line-feed. A UTF-8 byte-order mark is not accepted because it would become part of the first key.

The line grammar is:

```text
manifest    := logical-line ((LF | CR LF) logical-line)*
logical-line := empty | comment | key "=" value
comment     := "#" text
key         := one of the 50 keys listed below
value       := one-or-more bytes
```

The implementation removes a `CR` immediately preceding `LF`, so LF and CRLF line endings are accepted. A lone `CR`, including one at end of file, is invalid. A comment marker is recognised only as the first byte of a line; leading whitespace and inline comments are not supported. Comment text is discarded semantically but remains subject to the whole-file UTF-8 and control-byte rules. Empty lines are ignored. A non-comment line uses its first `=` as the separator. The key and value must both be non-empty. Space or tab may not surround either side of the separator, and a key may not contain whitespace. The parser does not trim values.

All 50 keys are mandatory and may occur in any order. A duplicate, unknown, or missing key invalidates the manifest. Except for numeric and exact-marker fields, values are non-empty, valid shortest-form UTF-8 without control bytes `U+0000` through `U+001F` or `U+007F`, and at most 4,096 encoded bytes. Comments are discarded and are not embedded in the package.

Layer and tile identifiers are at most 64 bytes and use only ASCII letters, digits, `.`, `_`, and `-`. SHA-256 values are exactly 64 lowercase hexadecimal digits. Unsigned sizes and dimensions are complete base-10 unsigned integer strings accepted by C++20 `std::from_chars`; signs and trailing characters are rejected. Floating-point fields and height tokens are complete finite values accepted by `std::from_chars` with `std::chars_format::general`.

### Required identity and terrain fields

| Key | Required value or constraint | Package meaning |
|:---|:---|:---|
| `format` | Exactly `game_ex_staging_manifest` | Staging schema identity |
| `version` | Exactly `1` | Staging schema version |
| `crs` | Exactly `EPSG:5514` | Numeric package CRS `5514` |
| `terrain.layer_count` | Exactly `1` | Current single-layer bound |
| `terrain.layer_id` | Portable identifier, at most 64 bytes | `TerrainTile::layer_id` |
| `terrain.tile_count` | Exactly `1` | Current single-tile bound |
| `terrain.tile_id` | Portable identifier, at most 64 bytes | `TerrainTile::tile_id` |
| `terrain.tile.columns` | Integer from 1 through 4,096 | Samples in each row |
| `terrain.tile.rows` | Integer from 1 through 4,096 | Number of rows |
| `terrain.tile.origin_x` | Finite double | EPSG:5514 X of column zero |
| `terrain.tile.origin_y` | Finite double | EPSG:5514 Y of row zero |
| `terrain.tile.origin_z` | Finite double | Absolute vertical-datum elevation used as the local-height origin |
| `terrain.tile.spacing_x` | Finite double greater than zero | Metres between adjacent columns |
| `terrain.tile.spacing_y` | Finite double greater than zero | Metres between adjacent rows |
| `terrain.tile.heights` | Safe portable relative path, at most 1,024 bytes | Height text input and embedded staged path |
| `terrain.tile.validity` | Safe portable relative path, at most 1,024 bytes | Validity text input and embedded staged path |

`columns * rows` must be no more than 4,194,304 and must fit the host `std::size_t`. The derived values `origin_x + spacing_x * (columns - 1)` and `origin_y + spacing_y * (rows - 1)` must be finite.

### Required source and provenance fields

| Key | Required value or constraint | Package meaning |
|:---|:---|:---|
| `provenance.provider` | General text | Supplying organisation or authority |
| `provenance.product` | General text | Provider-defined dataset/product |
| `provenance.edition` | General text | Edition, release, or snapshot |
| `provenance.source_uri` | General text | Source locator; it is not dereferenced by the portable compiler |
| `provenance.acquired_utc` | Real calendar value formatted `YYYY-MM-DDTHH:MM:SSZ` | UTC acquisition timestamp |
| `provenance.licence_name` | General text | Licence identifier or concise name |
| `provenance.licence_uri` | General text | Licence-text locator |
| `provenance.attribution` | General text | Attribution retained by consumers |
| `provenance.rights` | General text | Redistribution or rights statement |
| `provenance.source_filename` | Path-free ASCII name using only letters, digits, `.`, `_`, and `-`; neither `.` nor `..` | Original acquired-source filename |
| `provenance.source_byte_size` | Unsigned 64-bit integer | Asserted exact size of the original source |
| `provenance.source_sha256` | Lowercase SHA-256 | Asserted digest of the original source |
| `provenance.source_classification` | General text | Classification of the acquired source |
| `provenance.stage_classification` | General text | Classification of the normalised stage |
| `source.crs` | General text | Original source CRS identifier |
| `source.axis_order` | General text | Original source horizontal-axis order |
| `source.horizontal_units` | General text | Original source horizontal units |
| `source.vertical_reference` | General text | Original vertical reference, or an explicit not-applicable statement |
| `source.vertical_units` | General text | Original vertical units, or an explicit not-applicable statement |
| `source.no_data` | General text | Original no-data convention |

The source size and SHA-256 are preserved assertions. Version 1 has no acquisition adapter and does not open `provenance.source_uri` or recompute the original-source digest. The process that acquired or generated a real source is responsible for those values. By contrast, the staged height and validity files below are read and verified by this compiler.

### Required normalised-stage and staged-integrity fields

| Key | Required value or constraint | Package meaning |
|:---|:---|:---|
| `stage.axis_order` | Exactly `X,Y` | Columns advance +X and rows advance +Y |
| `stage.horizontal_units` | Exactly `metre` | Horizontal coordinate/spacing units |
| `stage.vertical_reference` | General text | Datum/reference used by `origin_z` |
| `stage.vertical_units` | Exactly `metre` | `origin_z` and local-height units |
| `stage.no_data` | Exactly `validity_mask` | No-data is independent of height values |
| `terrain.resolution` | General text | Human-readable resolution statement |
| `spatial.bounds` | General text | Declared bounds provenance; not runtime geometry |
| `spatial.epoch` | General text | Dataset epoch or explicit not-applicable value |
| `provenance.transformation` | General text | Ordered transformations, including tools and versions |
| `provenance.responsible_owner` | General text | Person or project role responsible for the derivative |
| `staged.heights.byte_size` | Unsigned 64-bit integer equal to the referenced file size | Verified height-text size |
| `staged.heights.sha256` | Lowercase SHA-256 equal to the referenced file digest | Verified height-text digest |
| `staged.validity.byte_size` | Unsigned 64-bit integer equal to the referenced file size | Verified validity-text size |
| `staged.validity.sha256` | Lowercase SHA-256 equal to the referenced file digest | Verified validity-text digest |

### Referenced staged files

Both staged paths use `/` as the only separator. They must be relative, non-empty, and no more than 1,024 bytes; may not begin or end with `/`; may not contain `//`, `\`, `:`, empty components, `.` components, or `..` components; and must resolve to regular files strictly beneath the canonical manifest directory. Canonical resolution also prevents a symlink from escaping that directory.

Each staged text file is limited to 64 MiB. Its exact bytes must match the declared byte size and SHA-256 before token parsing begins.

The height file contains exactly `columns * rows` whitespace-delimited finite binary32 values. The validity file contains the same number of tokens, each exactly `0` or `1`. Token whitespace is ASCII space, tab, carriage return, or line feed; physical line boundaries do not define rows. Token order defines the grid:

```text
index = row * columns + column
x = origin_x + spacing_x * column
y = origin_y + spacing_y * row
absolute_z = origin_z + double(height[index])
```

Columns advance in positive X and rows advance in positive Y. A validity value of `1` means the corresponding height is usable. A value of `0` means no-data; its paired height must still be finite but consumers must exclude it from valid geometry and statistics.

### Deterministic compilation and publication

`game_ex::world_compiler::load_staging_world` produces a logical `game_ex::world_format::WorldPackage` for validation callers. `game_ex::world_compiler::compile_world` accepts the manifest and output paths, performs that same stage load internally, validates the resulting model during encoding, and emits fields in the fixed order below. Manifest field order, comments, line-ending style, filesystem enumeration, locale, wall-clock time, and random state do not enter the output. Exact semantic field values and the embedded staged paths/sizes/digests do enter the output.

The output filename must have the lowercase `.gexworld` extension, and its
destination directory must already exist. Publication never replaces a path
owned by another process. The compiler first creates `<destination>.lock` as an
atomic, no-replace directory reservation. A lock it did not create is preserved
and causes refusal, whether active or stale. While holding its reservation, the
compiler refuses an existing destination or legacy sibling
`<destination>.part`. It writes and closes the complete bytes at its private
`<destination>.lock/package.part` path and publishes them by creating the
destination as a hard link to that file. Same-filesystem hard-link creation is the
atomic no-replace operation: if another actor creates the destination first, that
path is retained and compilation fails. The legacy sibling is never opened or
removed. The compiler then removes only `package.part` and the lock directory it
created. Successful compilation leaves neither owned artifact; cleanup failure
is reported. Consequently, the destination filesystem must support
same-filesystem hard links between the sibling lock directory and destination.

## `.gexworld` wire format 0.2

### Primitive encodings

All integer fields are unsigned and little-endian. `u16`, `u32`, and `u64` below mean 2-, 4-, and 8-byte integers respectively. `f32` and `f64` are IEEE-754 binary32 and binary64 bit patterns encoded in little-endian byte order. No C++ object representation, structure padding, or native endianness is written.

A metadata string is a little-endian `u32` byte length followed immediately by that many UTF-8 bytes. It has no terminator or alignment padding. Every string is non-empty, at most 4,096 bytes, valid shortest-form UTF-8, and contains no control byte below `0x20` or equal to `0x7f`.

### Canonical section layout

Let `S = columns * rows`, `H = 128`, and `M` be the encoded metadata-block size. The only valid offsets are:

```text
heights_offset  = H
validity_offset = H + 4*S
metadata_offset = H + 4*S + S
file_size       = H + 4*S + S + M
```

There is no padding, overlap, optional section, or trailing data. Only files from 128 bytes through 67,108,864 bytes inclusive enter structural validation; a valid file is necessarily large enough to contain its required sample and metadata payloads.

### Fixed header

| Offset | Bytes | Type | Required interpretation |
|------:|------:|:---:|:---|
| 0 | 8 | bytes | ASCII magic `GAMEEXWD` (`47 41 4d 45 45 58 57 44`) |
| 8 | 2 | `u16` | Major format version, exactly `0` |
| 10 | 2 | `u16` | Minor format version, exactly `2` |
| 12 | 4 | `u32` | Header size, exactly `128` |
| 16 | 8 | `u64` | Complete file size, exactly the actual byte count |
| 24 | 4 | `u32` | CRC-32/ISO-HDLC of the complete package with these four bytes treated as zero |
| 28 | 4 | `u32` | Reserved flags, exactly zero |
| 32 | 4 | `u32` | Horizontal EPSG code, exactly `5514` |
| 36 | 4 | `u32` | Tile columns |
| 40 | 4 | `u32` | Tile rows |
| 44 | 4 | `u32` | Metadata string count, exactly `35` |
| 48 | 8 | `f64` | EPSG:5514 X coordinate of the first sample |
| 56 | 8 | `f64` | EPSG:5514 Y coordinate of the first sample |
| 64 | 8 | `f64` | Positive X spacing in metres |
| 72 | 8 | `f64` | Positive Y spacing in metres |
| 80 | 8 | `u64` | Height payload offset, exactly `128` |
| 88 | 8 | `u64` | Validity payload offset, exactly `128 + 4*S` |
| 96 | 8 | `u64` | Metadata offset, exactly `128 + 5*S` |
| 104 | 8 | `u64` | Metadata block byte size `M` |
| 112 | 8 | `u64` | Sample count, exactly `columns * rows` |
| 120 | 8 | `f64` | Absolute vertical-datum origin Z |

The X, Y, Z, and spacing values must be finite; both spacings must be greater than zero; and both derived horizontal maximum bounds must be finite. Dimensions are each from 1 through 4,096 and their product is at most 4,194,304.

### Height and validity payloads

Starting at byte 128 are exactly `S` finite `f32` local-height offsets in row-major order. Starting immediately afterward are exactly `S` one-byte validity values. The only valid mask bytes are `0x00` and `0x01`.

Runtime geometry uses the same formulas as the staging contract. In particular, `origin_z` is not added during encoding: it remains an `f64`, each height remains a local `f32`, and a consumer performs `origin_z + double(local_height)` when it needs an absolute elevation. The package contains no trusted numeric bounds or precomputed vertical extrema.

### Metadata block

The metadata block begins with three integers:

| Relative offset | Type | Meaning |
|------:|:---:|:---|
| 0 | `u64` | Asserted original-source byte size |
| 8 | `u64` | Verified staged-height text byte size |
| 16 | `u64` | Verified staged-validity text byte size |

Immediately following those 24 bytes are exactly 35 length-prefixed strings in this order:

| Index | Logical member | Staging key |
|------:|:---|:---|
| 0 | `TerrainTile::layer_id` | `terrain.layer_id` |
| 1 | `TerrainTile::tile_id` | `terrain.tile_id` |
| 2 | `Provenance::provider` | `provenance.provider` |
| 3 | `Provenance::product` | `provenance.product` |
| 4 | `Provenance::edition` | `provenance.edition` |
| 5 | `Provenance::source_uri` | `provenance.source_uri` |
| 6 | `Provenance::acquisition_timestamp` | `provenance.acquired_utc` |
| 7 | `Provenance::licence_name` | `provenance.licence_name` |
| 8 | `Provenance::licence_uri` | `provenance.licence_uri` |
| 9 | `Provenance::attribution` | `provenance.attribution` |
| 10 | `Provenance::rights_statement` | `provenance.rights` |
| 11 | `Provenance::source_filename` | `provenance.source_filename` |
| 12 | `Provenance::source_sha256` | `provenance.source_sha256` |
| 13 | `Provenance::source_classification` | `provenance.source_classification` |
| 14 | `Provenance::stage_classification` | `provenance.stage_classification` |
| 15 | `Provenance::source_crs` | `source.crs` |
| 16 | `Provenance::source_axis_order` | `source.axis_order` |
| 17 | `Provenance::source_horizontal_units` | `source.horizontal_units` |
| 18 | `Provenance::source_vertical_reference` | `source.vertical_reference` |
| 19 | `Provenance::source_vertical_units` | `source.vertical_units` |
| 20 | `Provenance::source_no_data_convention` | `source.no_data` |
| 21 | `Provenance::stage_axis_order` | `stage.axis_order` |
| 22 | `Provenance::stage_horizontal_units` | `stage.horizontal_units` |
| 23 | `Provenance::stage_vertical_reference` | `stage.vertical_reference` |
| 24 | `Provenance::stage_vertical_units` | `stage.vertical_units` |
| 25 | `Provenance::stage_no_data_convention` | `stage.no_data` |
| 26 | `Provenance::resolution` | `terrain.resolution` |
| 27 | `Provenance::bounds` | `spatial.bounds` |
| 28 | `Provenance::epoch` | `spatial.epoch` |
| 29 | `Provenance::transformation` | `provenance.transformation` |
| 30 | `Provenance::responsible_owner` | `provenance.responsible_owner` |
| 31 | `Provenance::staged_heights_path` | `terrain.tile.heights` |
| 32 | `Provenance::staged_heights_sha256` | `staged.heights.sha256` |
| 33 | `Provenance::staged_validity_path` | `terrain.tile.validity` |
| 34 | `Provenance::staged_validity_sha256` | `staged.validity.sha256` |

The encoded metadata size must end exactly after string 34. Layer and tile identifiers retain their 64-byte restricted alphabet. The source filename remains path-free and restricted to letters, digits, `.`, `_`, and `-`. All three SHA-256 strings remain lowercase 64-digit hexadecimal values. The acquisition timestamp remains a validated real calendar value in `YYYY-MM-DDTHH:MM:SSZ` form.

The two staged paths are retained for audit, not opened by the runtime reader. The reader requires portable forward-slash relative spelling: no leading or trailing slash, `//`, backslash, colon, `.` component, or `..` component. Each remains subject to the general 4,096-byte package-string limit. The compiler applies the stricter 1,024-byte stage-input path limit before creating a package.

### CRC-32 integrity field

The checksum is CRC-32/ISO-HDLC with width 32, polynomial `0x04c11db7` (reflected implementation polynomial `0xedb88320`), initial value `0xffffffff`, reflected input and output, and final XOR `0xffffffff`. The canonical check value for the nine ASCII bytes `123456789` is `0xcbf43926`.

For a package, calculation covers every byte from offset zero through the end of the metadata block in order, except offsets 24 through 27 are supplied to the algorithm as four zero bytes regardless of their stored contents. The resulting `u32` is then stored little-endian at offset 24. A reader performs the same substitution and rejects a mismatch before interpreting the header or payload.

CRC-32 detects accidental corruption and incomplete mutation; it is not an authenticity signature. Source and staged SHA-256 provenance serve a different audit role and do not make the package cryptographically authenticated.

## Runtime validation and failure model

`game_ex::world_format::read_world_package` reads the bounded complete file and constructs a `WorldPackage` only after all checks succeed. It rejects:

- a file smaller than 128 bytes or larger than 64 MiB, an inaccessible file, or an incomplete read;
- a CRC-32 mismatch;
- wrong magic, any version other than `0.2`, a non-128-byte header, a declared size different from the actual file, non-zero flags, a CRS other than EPSG:5514, or a metadata-string count other than 35;
- zero or over-limit dimensions, a sample product above 4,194,304, a mismatched stored sample count, a non-finite coordinate/spacing/height, non-positive spacing, or a non-finite derived horizontal bound;
- any non-canonical section offset, section-size overflow, package-size overflow, overlap, gap, or trailing byte;
- a validity byte other than zero or one;
- a truncated, empty, oversized, invalid-UTF-8, or control-bearing metadata string;
- an invalid identifier, timestamp, source filename, digest, or staged relative path;
- stage semantics other than axis order `X,Y`, horizontal units `metre`, vertical units `metre`, and no-data convention `validity_mask`; and
- metadata that does not end exactly at the declared file end.

Failures use `game_ex::world_format::PackageError`. `PackageErrorCode::io` reports access or complete-read failure, `invalid_format` reports structural or value violations, `unsupported_version` reports a version mismatch, and `integrity` reports CRC failure. Allocation can separately raise `std::bad_alloc`, but the byte and sample limits are enforced before the principal allocations.

Runtime validation does not reacquire the original source, re-open staged paths, recompute provenance SHA-256 values, or trust the prose `spatial.bounds` as geometry. It validates their representation and returns them for audit. X/Y bounds are derived from numeric tile fields. Valid-height extrema, if needed, are derived only from mask entries equal to one.

## Compatibility rules

The authoritative constants are `game_ex::world_format::current_major_version == 0` and `current_minor_version == 2`. `read_world_package` accepts that exact pair. `game_ex::world_format::is_supported` checks only magic and major version on an already logical `WorldHeader`; it is not permission to decode an arbitrary minor layout and must not be used instead of `read_world_package` for files.

No reserved extension mechanism exists in format `0.2`: flags must be zero, counts and offsets must be exact, and trailing bytes are invalid. Any change to primitive interpretation, header fields, section ordering, metadata ordering, grid orientation, checksum coverage, or required values needs a new world-format version and an explicit decoder. A later reader may support multiple known versions, but it must implement each accepted layout explicitly rather than infer compatibility from a matching major number.

The architectural rationale and repository/dependency boundary are recorded in [ADR 0009](../decisions/0009-portable-terrain-stage-and-package.md) and [ADR 0005](../decisions/0005-world-compiler-repository-split-gate.md).
