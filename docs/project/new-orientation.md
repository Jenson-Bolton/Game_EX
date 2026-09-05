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
- SDL3 is a platform backend. OpenGL and Vulkan will be isolated behind separate backends implementing one shared Render API/RHI.
- Platform code exposes low-level services rather than game concepts such as save games.
- Subsystems are ordinary owned objects, not singleton “managers.”
- Startup dependencies form a validated directed acyclic graph scheduled in topological order; they are not implemented as depth-first singleton construction.
- The bounded job system is serial bootstrap infrastructure because parallel startup cannot schedule creation of its own executor.
- Controlled parallel startup enforces main-thread affinity and serial reverse-logical cleanup after every worker batch settles.
- Determinism and headless execution are explicit design properties.

The current source implements the small application/platform window foundation, bounded synchronous jobs, and validated serial/controlled-parallel startup paths. It does not pre-empt the pending specifications for any broader frame scheduler, the renderer, simulation, or editor UI.

The spelling “Czech Republic” is used throughout this project in accordance with the project owner's preference.
