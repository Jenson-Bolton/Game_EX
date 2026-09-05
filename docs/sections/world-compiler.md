# World compiler and format section

## Purpose

WorldCompiler converts large heterogeneous source datasets into validated runtime packages. WorldFormat is the narrow contract read by the game and editor.

## Implemented now

- `GameEX::WorldFormat`, containing the logical world model and bounded runtime package reader;
- `GameEX::WorldCompilerCore`, containing strict staging validation and deterministic compilation;
- a version-1 `.gexstage` manifest for exactly one EPSG:5514 terrain layer and tile;
- a concrete `.gexworld` 0.2 little-endian package with CRC-32 integrity, double world origin, local float heights, a validity mask, and embedded provenance;
- `validate`, `compile`, and `inspect` commands with stable categorized exit codes;
- a dependency-free synthetic fixture and focused format/compiler/CLI tests;
- standalone and workspace CMake integration with warning-as-error compilation and Doxygen.

The C++ structures are still semantic models, never disk layouts. The exact bytes and validation rules are defined in the [world package format](../architecture/world-package-format.md), while command usage is in the [WorldCompiler README](../../WorldCompiler/README.md).

This slice starts from already normalised staged text. It has no source adapter, CRS transformation, network access, GDAL/PROJ/Python dependency, TESSERA integration, compression, streaming index, reconciliation, or accepted real-world data. Those omissions are explicit boundaries rather than placeholder behaviour.

## Future sections

The next compiler-facing decision is whether to move acquisition and source-specific tooling into `Game_EX_WorldCompiler` before adding GDAL/PROJ, Python, network retrieval, or TESSERA. The first accepted data adapter should target the freshly identified CUZK DMR 5G `BYSH62` proof tile, retain the raw source outside ordinary Git, and present it separately in the editor before any layer combination.
