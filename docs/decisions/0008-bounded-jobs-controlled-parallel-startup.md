# ADR 0008: Bounded jobs and controlled parallel startup

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

[ADR 0006](0006-validated-serial-startup-graph.md) established whole-graph validation, deterministic serial topological order, partial-start rollback, and exact reverse cleanup before concurrency. The next lifecycle slice needs bounded worker execution without turning the startup graph into a service locator or making observable results depend on which worker happens to finish first.

Physical completion order cannot be deterministic under real concurrency. Game_EX therefore needs separate definitions for logical admission, result indexing, lifecycle commit, primary failure, diagnostics, and rollback. It also needs to keep window, renderer-presentation, and other thread-affine work on the application thread.

## Decision

Introduce an ordinarily owned, explicitly injected `JobSystem` for synchronous batches. It is a bounded execution mechanism for controlled engine work, not a global service, detached-task facility, continuation framework, or general asynchronous application API.

The configured worker count is immutable for one `JobSystem`, must be between 1 and 32 inclusive, and is rejected outside that range. When no count is specified, the recommended count is:

```text
clamp(hardware_concurrency - 1, 1, 8)
```

The implementation must evaluate the subtraction without unsigned underflow. A reported `hardware_concurrency` of zero or one therefore selects one worker. Tests and reproducible diagnostics pass an explicit worker count instead of depending on the host default.

### Synchronous batch contract

One call supplies an ordered batch and does not return until every admitted task has finished. A single job system executes one batch at a time; it has no unbounded backlog. Tasks and captured state remain owned by the caller for the whole call, no future or task handle escapes the barrier, and a task may not submit a nested batch to the same system.

Input position is the immutable logical job index. Workers may claim and physically complete jobs in any order, but each value, exception, and captured diagnostic is stored in its indexed slot. After all workers reach the barrier, the caller observes results and buffered diagnostics in ascending logical-index order. Worker code used by deterministic startup must not write directly to an order-sensitive shared log or commit central lifecycle state.

If several jobs fail, the exception at the lowest logical index is the primary batch failure. Other indexed failures remain available as secondary diagnostics. The first exception observed in wall-clock time has no special status.

### Controlled parallel startup

Each startup registration declares one execution class:

- `main_thread` runs only on the application thread;
- `worker_eligible` may run in a synchronous `JobSystem` batch after its declared dependencies have committed successfully.

The default is `main_thread` until a subsystem explicitly documents that its startup is safe for worker execution. Native window operations, event pumping, presentation/swapchain work, and other platform-affine operations remain `main_thread`.

The complete graph is still validated before side effects. Scheduling advances through lexical ready-set barriers:

1. Compute the currently ready set and sort it by stable subsystem ID.
2. If the first ready registration is `main_thread`, run exactly that node on the application thread, commit it, and recompute readiness. A failure prevents all later work from being admitted.
3. Otherwise logically admit the maximal stable-ID-ordered leading prefix of `worker_eligible` registrations, capped at the pool's worker count, as one synchronous batch. A later main-thread node or the capacity bound ends the prefix.
4. Wait for every admitted worker job, even after a worker failure. Do not admit later ready work while the batch remains in flight.
5. At the barrier, process indexed outcomes in logical admission order. If all succeeded, commit their lifecycle records in that order; otherwise choose the lowest-index failure as primary and stop further admission.
6. Recompute readiness only after a successful main-thread commit or worker-batch barrier.

Logical admission and commit order—not queue order, thread identity, or physical completion time—define observable startup order. Buffered startup diagnostics are emitted in logical order. Timing and worker identifiers may be retained as explicitly non-deterministic measurements but cannot select the primary failure or change lifecycle state.

On failure, all registrations whose `start()` was attempted, including a failing registration that may hold partial resources, enter the rollback ledger in deterministic logical admission order. After all in-flight work has stopped, cleanup runs serially on the application thread in exact reverse ledger order. Previously committed work follows the same reverse order. No `shutdown()` call runs concurrently, and cleanup continues after individual failures while preserving the deterministic primary startup failure.

The job system's own worker lifetime is established through a small serial bootstrap and is never scheduled on itself. It remains alive until startup rollback or normal subsystem shutdown has completed, then receives a stop request and joins every worker. Ownership is explicit; no subsystem obtains it through global lookup.

The creating application thread is the control and destruction thread for both `JobSystem` and `StartupGraph`. A wrong-thread destruction violates main affinity and can attempt a worker self-join, so the implementation ends the process immediately with failure before member cleanup rather than silently running cleanup on the wrong thread.

## Consequences

The serial behavior from ADR 0006 remains available and executes every registration on the caller regardless of its affinity declaration. Parallel speedup is limited to independent, explicitly eligible nodes in a bounded lexical-ready prefix. Barriers and main-thread interleaving may leave some worker capacity idle, but they make dependency visibility, failure selection, diagnostics, and rollback reviewable.

Subsystem authors must distinguish thread-safe preparation from publication of shared state. Physical side effects performed inside `start()` may occur in any order, so any order-sensitive publication must be deferred to the deterministic barrier commit or protected by a separately accepted contract.

Waiting for the whole failed batch can delay error reporting, but it prevents rollback racing in-flight startup. The hard worker bound avoids accidental oversubscription; the conservative recommended default leaves capacity for the application/main thread and unrelated operating-system work.

Any later asynchronous job graph, priorities, work stealing, cancellation, nested submission, frame scheduling, or dynamic worker resizing requires a new ADR. This decision authorises only the bounded synchronous batch and its controlled use by startup.

## Alternatives considered

Using physical completion order for logs, failure, or rollback would make identical inputs produce different lifecycle evidence. Cancelling a batch on the first observed exception cannot safely assume already-running jobs have stopped. Concurrent cleanup would add a second ordering problem and could violate main-thread affinity. A global thread pool or general future-based task API would expose a much wider lifetime contract than this startup slice needs. Keeping startup serial forever would preserve determinism but leave independent expensive initialisation unable to use available cores.
