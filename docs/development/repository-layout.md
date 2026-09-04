# Repository layout

```text
Engine/
|-- include/game_ex/      Public headers
|-- src/                  Private implementations
|-- tests/                Dependency-light engine unit tests
`-- CMakeLists.txt

WorldCompiler/
|-- include/game_ex/world_format/
|-- src/world_format/
|-- tools/
|-- tests/
`-- CMakeLists.txt

Game/
|-- include/game_ex/game/ Shared game-application API
|-- src/app/              Game composition root
|-- src/editor/           Editor composition root
|-- src/common/           Shared application composition
`-- CMakeLists.txt

docs/
|-- project/              Vision, scope, roadmap, vocabulary
|-- architecture/         Cross-section structure and dependency rules
|-- sections/             Responsibility/status of each programme
|-- development/          Build, code, testing, and contribution guidance
|-- decisions/            Architecture decision records
`-- doxygen/              Generated-reference entry/group declarations
```

Generated files and third-party downloads belong under `build`, never in source directories. Public headers use the `game_ex/...` include prefix. Source-only implementation details do not receive public headers.
