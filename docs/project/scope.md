# Current scope and specification gate

## Current `v0.1.7` milestone

The current foundation includes:

- independent Engine, WorldCompiler, and Game C++20 CMake projects in one
  workspace and existing GitHub history;
- ordinary `GameEX::Core`, `GameEX::Jobs`, and `GameEX::Startup` ownership with
  validated deterministic startup/rollback and bounded controlled parallelism;
- platform-neutral Window and Render contracts over private SDL3 integration;
- real OpenGL 4.6 Core and Vulkan 1.3 renderer targets;
- the same exact linear sRGB diagnostic clear semantics in game and editor;
- explicit `--renderer=opengl|vulkan|auto`, omission-as-auto, strict no-fallback
  explicit choices, and a narrow pre-visibility Vulkan-to-OpenGL auto fallback;
- deterministic Vulkan device/queue/surface/swapchain policy, synchronization2
  transfer clear plus shaderless dynamic-rendering raster clears, per-image
  presentation semaphores and image views, zero-extent/surface-change deferral,
  transactional recreation, and optional/required validation;
- deterministic unit, application-lifecycle, real backend, validation, explicit
  game/editor, and auto-selection smoke evidence;
- the runtime-only `.gexworld` 0.2 reader and dependency-free compiler for one
  strictly validated synthetic EPSG:5514 terrain tile;
- editor-only pre-window package loading and provenance logging;
- an owning, aspect-correct diagnostic raster bounded to `64 x 64`, with
  lower-row/+Y-up orientation, height/validity colours, deterministic overview
  reduction, and real OpenGL/Vulkan presentation;
- an executable link boundary that keeps editor interpretation out of `game_ex`
  and compiler implementation out of both applications;
- code/project/section/history documentation and strict Doxygen.

## Accepted boundaries

OpenGL and Vulkan implement one narrow Render API without leaking their native
types. A successful cross-technology claim currently means that the same owning
`RenderFrame`, including any linear diagnostic raster, reaches both backends and
each successfully presents. Shared integer layout fixes aspect and orientation;
this is semantic/command evidence, not a screenshot/pixel-equality claim. Native
frame failures are terminal for that renderer object. Automated runs require a
real presentation.

Vulkan is default-enabled for the supported Windows build and may be disabled
explicitly. Auto creates a fresh composition per attempt and never hides a
post-visibility, presentation, or shutdown failure. [ADR 0011](../decisions/0011-vulkan-transfer-clear-and-renderer-selection.md)
records the complete policy.

The raster path is a deliberately bounded inspection aid. It accepts at most
4,096 cells and uses native rectangle clears rather than a shader/resource
pipeline. [`ADR 0012`](../decisions/0012-bounded-shaderless-world-raster-inspection.md)
records the 2D plan-view interpretation, editor-only package boundary, mapping,
reduction, and performance limit. The game retains a clear-only foundation
frame and never links the editor implementation.

WorldCompiler remains in the monorepo while dependency-light. Its split gate is
mandatory before GDAL, PROJ, Python, network acquisition, machine-learning, or
TESSERA dependencies enter supported compiler work. No real source has yet been
accepted. The legacy coursework dataset remains rejected as canonical input.

## Explicitly out of scope

`v0.1.7` does not implement:

- shaders, SPIR-V, meshes, buffers, textures, depth, pipelines, descriptor
  models, cameras, render-world extraction, screenshots, or pixel comparison;
- `.gexworld` loading or terrain/validity visualisation in the player game;
- full-resolution raster upload, quantitative picking, a legend UI, pan/zoom,
  interactive or navigable 3D terrain, or multiple simultaneous layers;
- editor panels/toolkit, open/save documents, selection, commands, or undo/redo;
- real DMR, RÚIAN, ZABAGED, Dynamic World, TESSERA, or other source adapters;
- multi-tile/layer packages, compression, spatial indexing, streaming, or
  schema migration;
- a general frame-job scheduler, ECS, gameplay, simulation, input mapping,
  audio, save files, networking, scripting, or modding;
- non-Windows Vulkan build/runtime evidence or another GPU-vendor test matrix;
- fully signalled WSI retirement through swapchain-maintenance presentation
  fences;
- repository/submodule splitting.

## Next specification gate

The next source milestone reaches the existing WorldCompiler split gate. Before
specialised acquisition, LAS/LAZ, GDAL/PROJ, Python, network, or ML dependencies
enter supported source code, decide the compiler repository name, visibility,
package/submodule connection, independent versioning, and CI/release ownership.

The first real package must remain a separately inspectable DMR derivative and
must not imply that a reduced diagnostic raster is full-resolution evidence. A
separate decision remains necessary before:

1. adding external compiler dependencies or splitting WorldCompiler;
2. accepting a real source licence/edition/acquisition/checksum/CRS record;
3. storing large source data in Git, a release, cache, or Git LFS;
4. combining authoritative and supplemental layers;
5. choosing a full editor UI toolkit or detachable-window model;
6. replacing diagnostic clears with the scalable shader/texture path or
   interpreting the terrain as a navigable 3D view.
