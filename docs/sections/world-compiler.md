# World compiler and format section

## Purpose

WorldCompiler converts large heterogeneous source datasets into validated runtime packages. WorldFormat is the narrow contract read by the game and editor.

## Implemented now

- a logical `WorldHeader` with a signature and major/minor version;
- a compatibility check shared by compiler and consumers;
- a dependency-free unit test;
- a CLI entry point that states that no pipeline is configured.

The C++ struct is not an on-disk binary layout. Endianness, schema technology, checksums, spatial indexes, compression, tile addressing, and compatibility rules beyond the initial major-version boundary remain undecided.

## Future sections

Likely areas include ingest, geography, terrain, buildings, transport, semantics, validation, and packaging. They should be created only as real vertical slices are specified.
