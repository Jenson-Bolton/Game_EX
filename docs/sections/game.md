# Game section

## Purpose

The Game project owns domain simulation and the player-facing composition root. Expected future domains include world state, geography, transport, population, economy, land use, construction, buildings, simulation, and UI.

## Implemented now

`game_ex` composes the SDL platform, a selected OpenGL 4.6 Core or Vulkan 1.3
renderer, a `1280 x 720` resizable window, and the minimal engine loop. Omission
or `--renderer=auto` tries Vulkan first; explicit `opengl`/`vulkan` never falls
back. It reports attempts and the selected backend, presents the same shared
linear dark-blue foundation frame, and exits cleanly. The game does not yet load
a package. Its executable links `GameEX::GameApp` but not the editor-only
`GameEX::WorldEditorApp` or `GameEX::WorldCompilerCore`.

There is no gameplay, terrain view, mesh/shader/resource path, ECS, or simulation
loop yet. The foundation frame proves semantic cross-backend presentation, not
pixels or a render-world design. Editor package interpretation is not a shortcut
for game streaming. The loop must not grow into those systems before fixed-step
timing, job interaction, determinism, streaming, and extraction are specified.
