# Game_EX Game

This CMake project contains the user-facing executables built on the Engine and WorldFormat targets:

- `game_ex`, the eventual transport/city/land-use game;
- `game_ex_world_editor`, the authoring and inspection tool for world content.

At this milestone each executable is still a correctly owned SDL3 window and event loop. The shared WorldFormat dependency now includes the verified `.gexworld` reader, but neither executable loads or renders a package yet. The world editor currently lives here because it is a user-facing consumer of engine and world-format APIs; this is a reversible placement recorded in ADR 0003. Its separate target is the boundary for future editor-only libraries, which must never be linked by `game_ex`. There is no editor UI, OpenGL/Vulkan backend, simulation, or persistence yet.

See the workspace [game section](../docs/sections/game.md) and [world editor section](../docs/sections/world-editor.md).
