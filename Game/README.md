# Game_EX Game

This CMake project contains the user-facing executables built on the Engine and WorldFormat targets:

- `game_ex`, the eventual transport/city/land-use game;
- `game_ex_world_editor`, the authoring and inspection tool for world content.

At this milestone each executable owns an SDL3 window and can run the same linear dark-blue diagnostic frame through either the OpenGL 4.6 Core or Vulkan 1.3 backend. Use `--renderer=opengl`, `--renderer=vulkan`, or `--renderer=auto`; omission means `auto`. Explicit choices never fall back, while automatic selection may retry OpenGL only after a typed Vulkan availability or initialization failure before the first successful window show. Each attempt receives a fresh platform and application composition.

Renderer startup and a hidden presentation attempt precede window visibility. A zero drawable or recoverable surface change may defer that attempt until the visible loop, and automated runs fail unless they achieve a real presentation. Shutdown hides the window before destroying graphics state. The shared WorldFormat dependency includes the verified `.gexworld` reader, but neither executable loads or visualises a package yet. The world editor currently lives here because it is a user-facing consumer of engine and world-format APIs; this reversible placement is recorded in ADR 0003. Its separate target remains the boundary for future editor-only libraries, which must never be linked by `game_ex`. There is no editor UI, mesh/shader path, simulation, or persistence yet.

See the workspace [game section](../docs/sections/game.md) and [world editor section](../docs/sections/world-editor.md).
