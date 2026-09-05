# Game_EX World Compiler

The World Compiler validates one normalised terrain stage and writes a portable
`.gexworld` package. `GameEX::WorldFormat` is the public, compiler-independent
runtime reader boundary; `GameEX::WorldCompilerCore` and
`game_ex_world_compiler` remain offline-only dependencies.

This is a deliberately narrow vertical slice. It proves deterministic staging,
packaging, integrity checking, and reopening with the public reader. The checked-in
2×2 fixture is project-authored synthetic data, not real Bystřice pod Hostýnem
data and not output from TESSERA or an authoritative Czech dataset.

## Build and test

From the repository root with a C++20 compiler and CMake 3.25 or newer:

```powershell
cmake -S WorldCompiler -B WorldCompiler/build -DBUILD_TESTING=ON
cmake --build WorldCompiler/build --config Debug
ctest --test-dir WorldCompiler/build -C Debug --output-on-failure
```

MSVC builds use `/W4 /WX`; GCC- and Clang-compatible builds use `-Wall -Wextra
-Wpedantic -Wconversion -Wsign-conversion -Werror`. The implementation uses only
the C++20 standard library.

### Synthetic fixture and wire-format golden

`tests/fixtures/minimal/manifest.gexstage` and its three referenced text files
are the project-authored 2×2 synthetic source/stage fixture described by their
embedded provenance. `tests/fixtures/minimal/expected.gexworld.hex` is an
independently checked-in textual hexadecimal rendering of the complete canonical
WorldFormat 0.2 result: 1,151 decoded bytes with SHA-256
`A30C97F6283FE8E80165B500785B8B8625715FE5A29481E2ABC9FA691F13E5F3`.
Comments in the `.hex` file record those facts but are not package bytes.

The compiler pipeline test requires byte-for-byte equality with all 1,151 golden
bytes. A separate executable that links only `GameEX::WorldFormat` materialises
the textual golden in a test-owned temporary file and loads it through
`read_world_package`. The golden therefore locks both encoder output and the
compiler-independent runtime-reader boundary. It carries the same project-owned,
synthetic, no-separate-licence provenance as the stage; it is not real-world data
and does not grant external reuse rights.

## Commands

The executable is normally at
`WorldCompiler/build/Debug/game_ex_world_compiler.exe` on a multi-configuration
Windows build. Use the equivalent path for another generator.

```powershell
WorldCompiler/build/Debug/game_ex_world_compiler.exe --help
WorldCompiler/build/Debug/game_ex_world_compiler.exe --version
WorldCompiler/build/Debug/game_ex_world_compiler.exe validate --manifest WorldCompiler/tests/fixtures/minimal/manifest.gexstage
WorldCompiler/build/Debug/game_ex_world_compiler.exe compile --manifest WorldCompiler/tests/fixtures/minimal/manifest.gexstage --output WorldCompiler/build/synthetic.gexworld
WorldCompiler/build/Debug/game_ex_world_compiler.exe inspect --package WorldCompiler/build/synthetic.gexworld
```

`validate` parses every mandatory manifest value, resolves and verifies both
staged inputs, and parses every sample without writing output. `compile` repeats
that validation, creates canonical bytes, publishes a new `.gexworld` file, and
reopens it through `GameEX::WorldFormat`. `inspect` verifies package integrity
before reporting dimensions, derived numeric bounds, local and absolute height
ranges, all provenance, staged-input hashes, and the checksum.

Compilation refuses to overwrite an existing output, including when compiler
processes race. It first atomically creates the sibling directory
`output.gexworld.lock`; a pre-existing lock is treated as active or stale and is
never removed by a process that did not create it. While holding the reservation,
the compiler refuses an existing output or legacy sibling
`output.gexworld.part`. It writes the complete private partial file as
`output.gexworld.lock/package.part`, then publishes it with
`std::filesystem::create_hard_link`.
Hard-link creation is an atomic no-replace operation, so an output created by an
external process during compilation is preserved. An unrelated process creating
the legacy sibling after the check cannot be truncated or removed because it is
never used as the compiler's temporary file. The compiler then removes its
private partial link and lock directory. Cleanup errors are included in the
diagnostic; successful compilation leaves neither owned artifact. The output
filesystem must support hard links between the destination directory and its
child lock directory (that is, on the same filesystem or volume). A stale lock
may be removed manually only after confirming that no compiler process still
owns it.

### Stable exit categories

| Code | Category | Meaning |
|---:|:---|:---|
| 0 | success | The requested operation completed. |
| 2 | usage | The command or argument sequence was invalid. |
| 3 | invalid manifest | Manifest syntax, schema, metadata, or numeric bounds failed. |
| 4 | invalid staged input | A staged path, file, sample, size, or SHA-256 failed. |
| 5 | invalid package | Package format, version, bounds, or CRC-32 failed. |
| 6 | I/O | A required file could not be opened, read, written, or published. |
| 7 | unexpected | An allocation or unexpected implementation failure escaped. |

Diagnostics are written to standard error. Successful machine-readable summaries
are line-oriented `key=value` text on standard output.

## Staging directory and manifest

Use `manifest.gexstage` as the conventional manifest name. Parsing is
content-driven, so the library does not require that extension. Referenced paths
must use forward slashes, be relative to the manifest directory, contain no
empty, `.` or `..` component, drive/root prefix, colon, or backslash, and resolve
through canonical paths to a regular file beneath that directory. This also
rejects a symlink that escapes the stage. UTF-8 path bytes are converted
explicitly to the native filesystem representation; on Windows they do not pass
through the process ANSI code page, so valid non-ASCII staged filenames remain
addressable.

The manifest is valid UTF-8, at most 64 KiB, and consists of exact `key=value`
lines. Keys use the documented ASCII spellings. Blank lines and lines whose first
byte is `#` are allowed. Comment text is discarded semantically but remains
subject to whole-file UTF-8 and control-byte validation. LF and CRLF line endings
are accepted; a lone CR, C0 control other than line endings, DEL, surrounding
whitespace, unknown key, duplicate key, missing key, empty value, or malformed
UTF-8 is rejected. A value may contain further `=` bytes because the first `=`
separates the key.

Schema version 1 requires every key below; there are no implicit provenance or
coordinate defaults:

```ini
format=game_ex_staging_manifest
version=1
crs=EPSG:5514
terrain.layer_count=1
terrain.layer_id=synthetic_terrain
terrain.tile_count=1
terrain.tile_id=tile_0_0
terrain.tile.columns=2
terrain.tile.rows=2
terrain.tile.origin_x=-742000.125
terrain.tile.origin_y=-1045000.5
terrain.tile.origin_z=250.25
terrain.tile.spacing_x=2
terrain.tile.spacing_y=2
terrain.tile.heights=heights.txt
terrain.tile.validity=validity.txt
provenance.provider=Game_EX project
provenance.product=Portable compiler synthetic fixture
provenance.edition=1
provenance.source_uri=project://WorldCompiler/tests/fixtures/minimal/synthetic_source.txt
provenance.acquired_utc=2026-09-05T00:00:00Z
provenance.licence_name=No separate licence selected
provenance.licence_uri=project://licence/not-selected
provenance.attribution=Jenson Bolton / Game_EX
provenance.rights=Project-authored test fixture; external reuse rights not granted
provenance.source_filename=synthetic_source.txt
provenance.source_byte_size=34
provenance.source_sha256=d199cadd7445603633c4484be2d4fda706d66a51d7d694bb3e161dd956c87690
provenance.source_classification=project_authored_synthetic
provenance.stage_classification=normalised_synthetic
source.crs=EPSG:5514
source.axis_order=X,Y
source.horizontal_units=metre
source.vertical_reference=synthetic_local_datum
source.vertical_units=metre
source.no_data=none
stage.axis_order=X,Y
stage.horizontal_units=metre
stage.vertical_reference=synthetic_local_datum
stage.vertical_units=metre
stage.no_data=validity_mask
terrain.resolution=2m_x_2m
spatial.bounds=declared synthetic bounds; runtime derives numeric tile bounds
spatial.epoch=not_applicable
provenance.transformation=1:project-authored text;2:WorldCompiler manifest version 1
provenance.responsible_owner=Game_EX project
staged.heights.byte_size=11
staged.heights.sha256=49a790b33a9d79d67e4dfe69ba84211a9bdc9a16e1d411d69528b4999751c137
staged.validity.byte_size=8
staged.validity.sha256=bbc71dbbc0ee6439cf77b46a975bb8f08043049552f169c19900d273af5a4c8f
```

The compiler fixes normalised stage axis order to `X,Y`, horizontal and vertical
units to `metre`, and no-data representation to `validity_mask`. The stage
vertical reference remains mandatory and dataset-specific. `provenance.acquired_utc`
must be a real calendar/time value in `YYYY-MM-DDTHH:MM:SSZ` form. Every SHA-256
is exactly 64 lowercase hexadecimal digits. The offline encoder and public
runtime reader both enforce those four fixed stage strings; even a package with
a recomputed valid CRC cannot redefine their semantics.

The original-source identity, size, and digest are retained from the manifest;
the compiler cannot reopen an acquisition that is intentionally outside the
normalised stage. In contrast, it reads the staged height and validity files,
computes their byte sizes and SHA-256 digests itself, and requires exact matches
before parsing. Changing either staged file therefore invalidates the manifest.

### Terrain text files

The height file contains exactly `columns * rows` finite decimal binary32 values,
separated by ASCII whitespace. Values are row-major tile-local offsets. The
validity file contains the same number of whitespace-separated tokens, each
exactly `0` (invalid) or `1` (valid). Both text files are bounded to 64 MiB.

For a sample at zero-based `(column, row)`:

```text
index = row * columns + column
x = origin_x + spacing_x * column
y = origin_y + spacing_y * row
absolute_z = origin_z + double(local_height[index])  // only when validity[index] == 1
```

Columns increase in +X and rows increase in +Y. Spacing must be positive.
Runtime tile bounds are derived from the numeric double-precision origin and
spacing:

```text
min_x = origin_x
max_x = origin_x + spacing_x * (columns - 1)
min_y = origin_y
max_y = origin_y + spacing_y * (rows - 1)
```

`spatial.bounds` is an auditable declared source/stage statement only; the reader
does not trust or parse it as runtime geometry.

## `.gexworld` version 0.2

The first concrete serialized package format is WorldFormat 0.2. Every integer,
float, and double is encoded explicitly in little-endian order. The encoder never
writes a C++ structure or native memory layout. IEEE-754 binary32/binary64 support
is required and checked at compile time.

### Fixed 128-byte header

| Offset | Size | Encoding and value |
|---:|---:|:---|
| 0 | 8 | ASCII magic `GAMEEXWD` |
| 8 | 2 | u16 major version, `0` |
| 10 | 2 | u16 minor version, `2` |
| 12 | 4 | u32 header size, `128` |
| 16 | 8 | u64 complete file size |
| 24 | 4 | u32 CRC-32/ISO-HDLC |
| 28 | 4 | u32 flags, currently zero |
| 32 | 4 | u32 horizontal EPSG code, `5514` |
| 36 | 4 | u32 columns |
| 40 | 4 | u32 rows |
| 44 | 4 | u32 metadata-string count, `35` |
| 48 | 8 | binary64 origin X |
| 56 | 8 | binary64 origin Y |
| 64 | 8 | binary64 spacing X |
| 72 | 8 | binary64 spacing Y |
| 80 | 8 | u64 height-payload offset, canonically `128` |
| 88 | 8 | u64 validity-payload offset |
| 96 | 8 | u64 metadata-block offset |
| 104 | 8 | u64 metadata-block size |
| 112 | 8 | u64 sample count |
| 120 | 8 | binary64 absolute vertical-datum origin Z |

The CRC uses the standard CRC-32/ISO-HDLC parameters. It covers the entire file
while bytes 24–27 are treated as zero. The reader verifies CRC before exposing
any decoded payload.

### Canonical payload and metadata order

Immediately after the header are `sample_count` binary32 local heights, followed
by `sample_count` one-byte validity values. There is no padding or overlap.

The metadata block begins with three little-endian u64 values: original-source
byte size, staged-height byte size, and staged-validity byte size. It then contains
exactly 35 strings. Each is a little-endian u32 byte length followed by bounded,
valid UTF-8 bytes in this order:

1. layer ID
2. tile ID
3. provider
4. product
5. edition
6. source URI
7. acquisition UTC
8. licence name
9. licence URI
10. attribution
11. rights statement
12. original source filename
13. original source SHA-256
14. source classification
15. stage classification
16. source CRS
17. source axis order
18. source horizontal units
19. source vertical reference
20. source vertical units
21. source no-data convention
22. stage axis order
23. stage horizontal units
24. stage vertical reference
25. stage vertical units
26. stage no-data convention
27. resolution
28. declared bounds statement
29. epoch
30. ordered transformation/tool/version description
31. responsible owner
32. staged-height relative path
33. staged-height SHA-256
34. staged-validity relative path
35. staged-validity SHA-256

The reader rejects non-canonical offsets, unexpected trailing bytes, malformed
UTF-8, invalid masks, non-finite numbers, invalid metadata, unsupported versions,
bad CRC, and all configured dimension, sample-count, string-length, arithmetic,
or file-size bound violations before returning `WorldPackage`.

The runtime `GameEX::WorldFormat` target publicly exposes only
`include/game_ex/world_format/**` and compiles no encoder. The offline compiler
API is exported from a separate include root while retaining the spelling
`game_ex/world_compiler/world_compiler.hpp`; encoder source and its private
declaration belong only to `GameEX::WorldCompilerCore`.

## Bounds and current limitations

- exactly one terrain layer and one tile;
- EPSG:5514 horizontal world coordinates only;
- dimensions from 1 to 4096 per axis, at most 4,194,304 samples;
- each metadata string at most 4096 UTF-8 bytes;
- complete package at most 64 MiB;
- no acquisition, GDAL/PROJ adapter, reprojection, resampling, seam handling,
  multi-source reconciliation, building/road/semantic layers, or TESSERA inference;
- no editor or game rendering integration in this slice;
- no claim that the synthetic coordinate values describe a real place;
- no separate licence has been selected for the project-authored fixture, and its
  metadata explicitly does not grant external reuse rights.

Real data must not be substituted until its provider, product/edition, licence,
rights, acquisition, checksums, source/stage classifications, horizontal and
vertical references, no-data rules, resolution, bounds/epoch, transformations,
and responsible owner have been verified and entered explicitly.
