# Game section

## Purpose

The Game project owns domain simulation and the player-facing composition root. Expected future domains include world state, geography, transport, population, economy, land use, construction, buildings, simulation, and UI.

## Implemented now

`game_ex` composes the SDL platform backend, the OpenGL 4.6 Core renderer, a `1280 x 720` resizable window, and the minimal engine application loop. It prints the shared world-format version and selected `opengl` backend, presents the shared linear dark-blue diagnostic clear, and exits cleanly on a close request. `GameEX::WorldFormat` supplies a verified package reader, but the game composition root does not yet load a package.

There is no gameplay, mesh/shader/resource path, ECS, or simulation loop yet. The diagnostic frame is renderer proof rather than a render-world design. The application loop must not be expanded into those systems before fixed-step timing, job interaction, determinism, and render extraction are specified.
