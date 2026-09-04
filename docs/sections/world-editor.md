# World editor section

## Purpose

The world editor will author, inspect, validate, and preview world content while using the same engine and runtime world contract as the game.

## Implemented now

`game_ex_world_editor` composes a distinct SDL application identity, a `1440 x 900` resizable top-level window, and the shared minimal event loop. Its distinct executable provides the target boundary where editor-only code can be isolated. As the editor grows, separate editor libraries and dependency checks must enforce that the game target does not link them.

## Specification needed next

- initial user workflow and smallest useful editable object;
- single docked window versus detachable native windows;
- UI toolkit and styling approach;
- document/open/save model and temporary recovery;
- undo/redo command boundaries;
- how the editor invokes or observes offline world compilation;
- live game preview in-process, another window, or another process;
- validation/error presentation and source provenance;
- runtime OpenGL/Vulkan selection, diagnostics, and the required degree of feature parity.
