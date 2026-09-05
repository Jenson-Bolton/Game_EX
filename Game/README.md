# Game_EX Game

This CMake project contains the user-facing executables built on the Engine and WorldFormat targets:

- `game_ex`, the eventual transport/city/land-use game;
- `game_ex_world_editor`, the authoring and inspection tool for world content.

At this milestone each executable owns an SDL3 window and can render the same backend-neutral diagnostic frame through either the OpenGL 4.6 Core or Vulkan 1.3 backend. Use `--renderer=opengl`, `--renderer=vulkan`, or `--renderer=auto`; omission means `auto`. Explicit choices never fall back, while automatic selection may retry OpenGL only after a typed Vulkan availability or initialization failure before the first successful window show. Each attempt receives a fresh platform and application composition.

Renderer startup and a hidden presentation attempt precede window visibility. A zero drawable or recoverable surface change may defer that attempt until the visible loop, and automated runs fail unless they achieve a real presentation. Shutdown hides the window before destroying graphics state.

The world editor accepts one optional `--world=<path>` argument. With no world it displays the foundation shell. With a package it reads and validates the complete `.gexworld` file before creating a platform or window, then visualizes the terrain height and validity layers as the same small linear-colour raster on both graphics backends. Valid heights use a global blue-green-yellow ramp; invalid samples are magenta. Larger inputs are independently downsampled to at most 64 columns and 64 rows with complete floor-division bins and valid-sample means. The display preserves the half-cell-padded `columns * spacing_x` by `rows * spacing_y` sample footprint, with source row zero at the lower edge and +Y upward.

Before reduction, each source sample has a conceptual cell centred on its
package coordinate, extending half a spacing beyond each outer sample. Reduced
display cells summarize complete source partitions while retaining that full
footprint. Package numeric bounds continue to describe the first through last
sample centres as `(dimension - 1) * spacing`; the viewer does not redefine
those world bounds.

For example, after compiling a package with the documented world compiler:

```powershell
.\build\bin\Debug\game_ex_world_editor.exe --world=C:\path\tile.gexworld --renderer=opengl
.\build\bin\Debug\game_ex_world_editor.exe --world=C:\path\tile.gexworld --renderer=vulkan
```

Package identity, checksum, dimensions, validity and height ranges, spatial metadata, source classification, provider, edition, attribution, and exact visualization rules are written to the console before rendering.

Editor-only loading and raster preparation live in `GameEX::WorldEditorApp`, which is linked only by `game_ex_world_editor`. The common `GameEX::GameApp` retains renderer selection and lifecycle composition, while `game_ex` never links editor implementation or `GameEX::WorldCompilerCore`. The editor consumes only the runtime `GameEX::WorldFormat` boundary. The world editor remains in this project because it is a user-facing consumer of engine and world-format APIs; this reversible placement is recorded in ADR 0003. There is no authoring UI, mesh/shader path, simulation, or persistence yet.

See the workspace [game section](../docs/sections/game.md) and [world editor section](../docs/sections/world-editor.md).
