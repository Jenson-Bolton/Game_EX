# Game section

## Purpose

The Game project owns domain simulation and the player-facing composition root. Expected future domains include world state, geography, transport, population, economy, land use, construction, buildings, simulation, and UI.

## Implemented now

`game_ex` composes the SDL platform backend, a `1280 x 720` resizable window, and the minimal engine application loop. It prints the shared logical world-format version for diagnostics and exits cleanly on a close request.

There is no gameplay, renderer, ECS, or simulation loop yet. The application loop must not be expanded into those systems before fixed-step timing, job interaction, determinism, and render extraction are specified.
