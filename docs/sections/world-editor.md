# World editor section

## Purpose

The world editor will author, inspect, validate, and preview world content while using the same engine and runtime world contract as the game.

## Implemented now

`game_ex_world_editor` composes a distinct SDL application identity, the same
selectable OpenGL 4.6 Core/Vulkan 1.3 backends as the game, a `1440 x 900`
resizable top-level window, and the exact shared diagnostic clear/present loop.
Its distinct executable is the boundary for editor-only code. The shared
`GameEX::WorldFormat` library can decode the package that the editor will inspect,
but this executable does not yet open or visualise it. As it grows, separate
editor libraries and dependency checks must prevent the game linking editor code.

## Specification needed next

- initial user workflow and smallest useful editable object;
- single docked window versus detachable native windows;
- UI toolkit and styling approach;
- document/open/save model and temporary recovery;
- undo/redo command boundaries;
- command-line path selection and how the editor invokes or observes offline world compilation;
- live game preview in-process, another window, or another process;
- validation/error presentation and source provenance;
- feature/capture parity beyond the current shared semantic frame and successful
  OpenGL/Vulkan presentations.

The next editor slice can load one synthetic `.gexworld` package through
`GameEX::WorldFormat` and display terrain/validity data under both backends. It
must remain a separate-layer view; combining geographic sources or claiming a
real source is not part of that rendering step.
