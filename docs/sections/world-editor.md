# World editor section

## Purpose

The world editor will author, inspect, validate, and preview world content while using the same engine and runtime world contract as the game.

## Implemented now

`game_ex_world_editor` composes a distinct SDL application identity, the same
selectable OpenGL 4.6 Core/Vulkan 1.3 backends as the game, and a `1440 x 900`
resizable top-level window. With no world it retains the foundation shell. One
optional `--world=<path.gexworld>` is read and verified through
`GameEX::WorldFormat` before desktop composition, then its terrain height and
validity become an owning aspect-correct diagnostic raster.

The aspect belongs to the conceptual unreduced colour-cell footprint: each
source sample receives one cell, so the full grid covers
`columns * spacing_x` by `rows * spacing_y` and extends half a spacing beyond
the outer sample centres. Reduced display cells summarize source partitions
while retaining that footprint. The package's numeric bounds remain the
first-to-last centre span, `(dimension - 1) * spacing`.

Valid source heights are globally normalised through a linear
blue-green-yellow ramp. Invalid cells are vivid magenta; partially valid reduced
bins blend toward magenta; a flat valid field uses the midpoint. Source row zero
is the lower edge and +Y points upward. Each source axis is independently
reduced to at most 64 cells by complete floor-partition bins with valid-sample
means. The console reports package identity/checksum, source/display dimensions,
validity/ranges, CRS/spacing, source classifications, provider/edition,
attribution, aspect, mapping, aggregation, and orientation.

`GameEX::WorldEditorApp` owns package interpretation and is linked only by the
editor executable. `game_ex` links only the common desktop application and does
not contain editor code. Neither application links `GameEX::WorldCompilerCore`.
OpenGL scissored clears and Vulkan dynamic-rendering attachment clears display
the same bounded raster without selecting a shader/resource interface.

## Specification needed next

- initial editing workflow and smallest useful editable object;
- single docked window versus detachable native windows;
- UI toolkit and styling approach;
- document/open/save model and temporary recovery;
- undo/redo command boundaries;
- file-picker/document selection and how the editor invokes or observes offline world compilation;
- live game preview in-process, another window, or another process;
- validation/error presentation and source provenance;
- feature/capture parity beyond the current shared semantic frame and successful
  OpenGL/Vulkan presentations.

The implemented view remains a static bounded 2D diagnostic. Full-resolution
texture upload, legend/picking, pan/zoom, 3D terrain/camera/depth, multiple
layers, editing, and source combination remain unimplemented. The next data
slice may use it to inspect a reduced real DMR overview separately, but cannot
claim quantitative or full-resolution visual evidence.
