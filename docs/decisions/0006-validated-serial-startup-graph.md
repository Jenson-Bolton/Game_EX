# ADR 0006: Validated serial startup graph

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

Game_EX needs deterministic subsystem startup and shutdown before job scheduling, rendering, world data, or simulation introduces more lifetimes. The earlier project used manager-style global access; the restart instead requires ordinary ownership and explicit dependencies. A dependency graph that discovers an error after partially starting the process would make failures difficult to reproduce and clean up.

## Decision

Introduce `GameEX::Startup` as a dependency-light C++20 library. A `StartupGraph` owns each `Subsystem` through `std::unique_ptr` and stores a non-empty stable identifier plus stable dependency identifiers. It exposes lifecycle orchestration, not subsystem lookup.

Registrations reject null implementations, empty identifiers, duplicate subsystem identifiers, and empty or duplicate dependency identifiers. Before the first start call, the graph resolves every dependency and computes a full topological order; missing dependencies and cycles therefore have no runtime side effects. Among simultaneously ready nodes, lexical stable-ID order provides a deterministic serial tie-break independent of registration order.

Orderly shutdown uses the exact reverse startup order. The currently starting subsystem is recorded before its `start()` call so rollback includes it when startup fails after acquiring only part of its resources. Cleanup continues after individual shutdown failures, while explicit shutdown reports the first failure. The graph is single-use and protects invalid transitions. Destruction attempts non-throwing cleanup only for a still-running graph.

The application registers native-window visibility as the first production subsystem. This proves integration without moving native platform ownership into the graph or inventing placeholder managers. Startup remains serial in this release.

## Consequences

Whole-graph structural errors are reported before side effects, lifecycle order is reproducible, and partial initialisation has a specified rollback path. Subsystems receive collaborators through construction and cannot use the graph as a service locator.

Subsystem `shutdown()` implementations must tolerate a call after their own `start()` throws. A cleanup failure during startup rollback cannot replace the original startup exception; diagnostics will later need a separate aggregation channel. The lexical tie-break is a deterministic policy, not a priority system.

A later job-system release may add controlled concurrent execution for worker-affine ready nodes. It must preserve pre-validation, stop admission after failure, await in-flight work, retain main-thread affinity, and define a deterministic rollback order before changing this contract.

## Alternatives considered

Constructor-driven global singletons obscure ownership and dependency errors. Registering callbacks without owned subsystem objects weakens lifetime and rollback contracts. Starting nodes while discovering the graph can leave the process partially active on a missing dependency or cycle. Parallelising immediately would combine graph correctness, scheduling, affinity, and failure races in one unproven change.
