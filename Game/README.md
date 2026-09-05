# Game_EX Game

This CMake project contains the user-facing executables built on the Engine and WorldFormat targets:

- `game_ex`, the eventual transport/city/land-use game;
- `game_ex_world_editor`, the authoring and inspection tool for world content.

At this milestone each executable owns an SDL3 window, the shared OpenGL 4.6 Core backend, and the same linear dark-blue diagnostic clear/present loop. Renderer startup and the first clear happen while the window is hidden; shutdown hides it before destroying graphics state. The shared WorldFormat dependency includes the verified `.gexworld` reader, but neither executable loads or visualises a package yet. The world editor currently lives here because it is a user-facing consumer of engine and world-format APIs; this reversible placement is recorded in ADR 0003. Its separate target remains the boundary for future editor-only libraries, which must never be linked by `game_ex`. There is no editor UI, Vulkan backend, mesh/shader path, simulation, or persistence yet.

See the workspace [game section](../docs/sections/game.md) and [world editor section](../docs/sections/world-editor.md).
