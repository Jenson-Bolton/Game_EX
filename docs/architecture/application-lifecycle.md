# Application lifecycle

The current lifecycle is intentionally synchronous and small:

```text
executable composition root
        |
        v
create SDL3 Platform
        |
        v
create hidden Window
        |
        v
Application::run
  | show window
  | pump events
  | idle briefly
  ` stop on close request
        |
        v
destroy Window, then Platform/SDL3
```

Member declaration order ensures that the native window is destroyed before its SDL runtime. Platform creation, event pumping, showing, and destruction occur on the same main thread.

The current `automatic_exit_after` setting is an automation hook for smoke tests, not a gameplay timer or public command-line design.

## Planned lifecycle, pending specification

The earlier Game_EX design calls for a small serial bootstrap (diagnostics, configuration, CPU/platform discovery), followed by job-system creation and a validated startup DAG. Ready nodes are scheduled topologically subject to thread affinity; shutdown follows reverse topological order.

The graph must detect missing dependencies, duplicates, and cycles before starting work. It must not rely on global singleton construction. These requirements are documented now so the foundation does not accidentally grow into the old singleton-manager design, but implementation waits for an accepted job and failure model.
