# ADR 0003: Initial world-editor placement

- Status: Accepted
- Date: 2026-09-04

## Context

The requested foundation needs a world-editor window while retaining the accepted Engine, WorldCompiler, and Game project split. The editor needs engine/window services and the runtime world contract; the compiler must remain usable offline and headless.

## Decision

Place `game_ex_world_editor` in the Game project as a separate executable. It may share small composition and domain libraries with the game but must not add editor-only code to the shipped `game_ex` binary. WorldCompiler remains the home of offline ingestion and packaging.

## Consequences

This keeps the three-project graph acyclic for the first milestone. It leaves open whether a mature editor becomes a fourth project after its UI, document, and compiler-orchestration requirements are understood.

## Alternatives considered

Placing the editor inside WorldCompiler would couple a headless data pipeline to engine/window services. Creating a fourth repository now would decide packaging and ownership before editor responsibilities are known.
