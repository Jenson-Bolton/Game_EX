# Game/editor boundary

The game and world editor are separate executables that share one desktop composition helper. Both consume Engine and WorldFormat, directly select the same OpenGL backend for this slice, and present the same diagnostic clear; neither contains SDL or OpenGL code.

The editor is placed in the Game project for the foundation because it is a user-facing consumer of the engine and runtime world contract, while the WorldCompiler remains an offline data-building programme. This placement does not mean editor UI or mutable authoring data belongs in the shipped game binary.

Future separation should preserve these rules:

- editor-only UI, commands, inspectors, import controls, and diagnostics do not ship in the game executable;
- runtime world structures remain usable without editor state;
- offline compilation remains callable without a graphics system;
- common domain operations should be factored by capability rather than copied between executables;
- undo/redo, document state, live preview, and compiler orchestration require explicit specifications.

The editor currently has one top-level native window. Whether it becomes a docked single-window UI, supports detached native panels, or controls a separate game preview process is the next specification decision.
