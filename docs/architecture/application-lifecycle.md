# Application lifecycle

The current lifecycle is intentionally serial and small:

```text
executable composition root
        |
        v
create SDL3 Platform
        |
        v
create hidden Window and owned StartupGraph
        |
        v
Application::run
  | validate entire graph
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

Application currently registers window visibility as its first real subsystem. Member declaration order ensures that the startup graph finishes cleanup before the native window is destroyed, and that the window is destroyed before its SDL runtime. Platform creation, event pumping, showing, hiding, and destruction occur on the same main thread.

The current `automatic_exit_after` setting is an automation hook for smoke tests, not a gameplay timer or public command-line design.

## Next lifecycle slice, pending specification

The next slice introduces an ordinarily owned job system and controlled parallel startup while preserving the proven validation, deterministic serial path, reverse cleanup, and failure semantics. Main-thread-affine work such as window and renderer setup must remain on the application thread. Worker-ready nodes may run concurrently only after their dependencies complete; failure must stop new admissions and wait for in-flight work before deterministic rollback.
