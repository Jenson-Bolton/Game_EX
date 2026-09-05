# Game section

## Purpose

The Game project owns domain simulation and the player-facing composition root. Expected future domains include world state, geography, transport, population, economy, land use, construction, buildings, simulation, and UI.

## Implemented now

`game_ex` composes the SDL platform, a selected OpenGL 4.6 Core or Vulkan 1.3
renderer, a `1280 x 720` resizable window, and the minimal engine loop. Omission
or `--renderer=auto` tries Vulkan first; explicit `opengl`/`vulkan` never falls
back. It reports attempts and the selected backend, presents the same shared
linear dark-blue diagnostic frame, and exits cleanly. `GameEX::WorldFormat`
supplies a verified package reader, but the game does not yet load a package.

There is no gameplay, mesh/shader/resource path, ECS, or simulation loop yet.
The diagnostic frame proves semantic cross-backend presentation, not pixels or
a render-world design. The loop must not grow into those systems before
fixed-step timing, job interaction, determinism, and extraction are specified.
