# New orientation

This restart is based on the earlier Game_EX and TESSERA discussion, not on extending the university graphics coursework repository. Coursework techniques and code may be evaluated later, but nothing is copied automatically.

The agreed structural direction is:

```text
Game_EX workspace
|-- Engine          reusable engine modules
|-- WorldCompiler   offline compiler plus exported WorldFormat
`-- Game            game and world-editor consumers
```

The important corrections to the original design are:

- `App` is a tiny composition root that selects implementations; it is not merely a large `main.cpp`.
- Engine capabilities are separate CMake targets rather than one monolithic library.
- SDL3 is a platform backend, and Vulkan will be isolated behind a future RHI backend.
- Platform code exposes low-level services rather than game concepts such as save games.
- Subsystems are ordinary owned objects, not singleton “managers.”
- Startup dependencies form a validated directed acyclic graph scheduled in topological order; they are not implemented as depth-first singleton construction.
- Bootstrap is initially serial because the job system must exist before parallel startup can use it.
- Thread affinity and reverse-order shutdown are part of the eventual startup graph.
- Determinism and headless execution are explicit design properties.

The current source implements only the smallest application/platform slice required to open the requested windows. It does not pre-empt the pending specifications for the startup graph, jobs, renderer, simulation, or editor UI.

The spelling “Czech Republic” is used throughout this project in accordance with the project owner's preference.
