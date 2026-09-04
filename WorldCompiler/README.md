# Game_EX World Compiler

This independent CMake project owns two related outputs:

- `GameEX::WorldFormat`, the small runtime-readable data contract used by the game;
- `game_ex_world_compiler`, the offline executable that will eventually ingest and reconcile authoritative geographic, terrain, building, transport, and semantic sources.

The executable currently reports that no pipeline is configured. It does not fabricate a file format or data-ingestion design before those specifications are agreed. The logical version header exists only to establish the compiler/runtime dependency boundary; its serialized representation is intentionally undecided.

See the workspace [world compiler section](../docs/sections/world-compiler.md) and [world data pipeline](../docs/architecture/world-data-pipeline.md).
