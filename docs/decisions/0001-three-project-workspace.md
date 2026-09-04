# ADR 0001: Three-project workspace

- Status: Accepted
- Date: 2026-09-04

## Context

Game_EX needs a reusable engine, a substantial offline world-data toolchain, and a game that consumes both without collapsing their responsibilities into one monolith.

## Decision

Maintain three independently configurable CMake projects: Engine, WorldCompiler, and Game. A lightweight root workspace composes all three for local development. WorldCompiler exports WorldFormat; the Game consumes Engine and WorldFormat, while the offline compiler executable does not become a shipped runtime component.

The current restart remains in the existing Game_EX GitHub repository until remote naming, package versioning, and submodule policy are specified.

## Consequences

Engine code stays domain-agnostic, world processing can evolve offline, and game code sees only runtime contracts. A later physical repository split requires installable CMake packages and coordinated versioning.

## Alternatives considered

A single monolithic project was rejected because it encourages accidental backend/domain coupling. Immediately creating three remotes was deferred because repository operations and compatibility policy have not been specified.
