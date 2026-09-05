# Game/editor boundary

The game and world editor are separate executables that share one desktop
composition helper and strict OpenGL/Vulkan/auto selection policy. Neither
contains direct SDL, OpenGL, or Vulkan code. The game submits the clear-only
foundation frame. The editor may instead submit an owning bounded colour raster
derived from a runtime world package; both frames use the same neutral Render
contract and real backend implementations.

The editor is placed in the Game project for the foundation because it is a
user-facing consumer of the engine and runtime world contract, while the
WorldCompiler remains an offline data-building programme. `GameEX::WorldEditorApp`
is linked only by `game_ex_world_editor`; `game_ex` links the common
`GameEX::GameApp` but not editor implementation. Only the editor target links
`GameEX::WorldFormat` in this slice, and neither application links
`GameEX::WorldCompilerCore`. Package loading, integrity checks, and mapping
complete before the shared helper creates a platform or renderer.

This placement does not mean editor UI or mutable authoring data belongs in the
shipped game binary. The dependency rule is verified from generated target link
interfaces and production executable closure rather than inferred from source
directories.

Future separation should preserve these rules:

- editor-only UI, commands, inspectors, import controls, and diagnostics do not ship in the game executable;
- runtime world structures remain usable without editor state;
- offline compilation remains callable without a graphics system;
- common domain operations should be factored by capability rather than copied between executables;
- undo/redo, document state, live preview, and compiler orchestration require explicit specifications.

The editor currently has one top-level native window and a static plan-view
diagnostic. Whether it becomes a docked single-window UI, supports detached
native panels, controls a separate game preview process, or first grows a 2D or
3D interactive viewport is the next specification decision.
