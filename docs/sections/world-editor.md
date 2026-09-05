# World editor section

## Purpose

The world editor will author, inspect, validate, and preview world content while using the same engine and runtime world contract as the game.

## Implemented now

`game_ex_world_editor` composes a distinct SDL application identity, a `1440 x 900` resizable top-level window, and the shared minimal event loop. Its distinct executable provides the target boundary where editor-only code can be isolated. The shared `GameEX::WorldFormat` library can now decode the package that the editor will inspect, but this executable does not yet open or visualise it. As the editor grows, separate editor libraries and dependency checks must enforce that the game target does not link them.

## Specification needed next

- initial user workflow and smallest useful editable object;
- single docked window versus detachable native windows;
- UI toolkit and styling approach;
- document/open/save model and temporary recovery;
- undo/redo command boundaries;
- command-line path selection and how the editor invokes or observes offline world compilation;
- live game preview in-process, another window, or another process;
- validation/error presentation and source provenance;
- runtime OpenGL/Vulkan selection, diagnostics, and the required degree of feature parity.

The next accepted vertical slice is intentionally smaller than a full editor UI: use the same shared Render API as the game, load one `.gexworld` package through `GameEX::WorldFormat`, and display its terrain/validity data as a diagnostic view under both OpenGL and Vulkan. It must remain a separate-layer view; combining geographic sources is not part of that rendering step.
