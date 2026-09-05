# Application lifecycle

The lifecycle retains a deterministic serial path and adds a bounded controlled-parallel path; the current application uses the latter:

```text
executable composition root
        |
        v
create SDL3 Platform
        |
        v
create hidden Window, bounded JobSystem, and owned StartupGraph
        |
        v
Application::run
  | validate job pool and entire graph
  | start window-visibility subsystem
  | pump events
  | idle briefly
  | stop on close request
  ` reverse-order subsystem shutdown
        |
        v
destroy Window, then Platform/SDL3
```

`GameEX::Startup` owns ordinary subsystem objects and their stable dependency declarations. It validates missing dependencies and cycles before invoking any `start()` method. Ready nodes use stable lexical ordering, so serial startup does not depend on registration or filesystem order. Normal shutdown exactly reverses successful startup. If a subsystem throws during `start()`, rollback first invokes `shutdown()` on that potentially partial subsystem and then on every earlier subsystem in reverse order.

The graph is single-use and protects its lifecycle states. Validation errors leave it configurable because no work has started; runtime start or shutdown failures are terminal. Destruction attempts non-throwing cleanup for a graph still running, while an explicit `shutdown()` reports the first cleanup exception after attempting every remaining cleanup.

Application currently registers window visibility as its first real subsystem and marks it main-thread-affine. The application owns a conservative fixed worker pool before its startup graph, so graph cleanup completes before workers join, the native window is then destroyed, and SDL shuts down last. Platform creation, event pumping, showing, hiding, and destruction occur on the same main thread.

The current `automatic_exit_after` setting is an automation hook for smoke tests, not a gameplay timer or public command-line design.

## Controlled parallel startup

[ADR 0008](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) specifies the implemented ordinarily owned job system with 1–32 workers and synchronous indexed batches. The serial graph path remains available and full pre-validation is unchanged. A lexical ready set governs admission. If its first node is main-thread-affine, that one node runs on the owner and readiness is recomputed. Otherwise the maximal lexical-leading worker-eligible prefix, capped by worker count, runs as one batch and reaches a barrier before readiness is recomputed.

Physical worker completion is intentionally non-deterministic. Logical indices control result placement, primary-failure selection, lifecycle commit, and reverse rollback. After a failure, every admitted job is awaited, later work is not admitted, and cleanup runs serially on the application thread.

Both the pool and graph must be destroyed on their creating application thread. The implementation ends the process immediately with failure before cleanup if ownership is transferred to another thread for destruction, preventing main-affine shutdown from running on a foreign thread or a worker from joining itself.
