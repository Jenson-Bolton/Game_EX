# Repository layout

```text
Engine/
|-- include/game_ex/      Public headers
|-- src/                  Private implementations
|-- tests/                Dependency-light engine unit tests
`-- CMakeLists.txt

WorldCompiler/
|-- include/game_ex/world_format/       Runtime model and reader
|-- src/world_format/                   Runtime reader internals
|-- src/compiler/include/game_ex/world_compiler/
|                                       Offline-only compiler API
|-- src/compiler/                       Parser, encoder, and publisher
|-- tools/                              CLI composition root
|-- tests/                              Format/compiler/CLI tests and synthetic fixture
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
|-- history/              Version reports and forensic project records
`-- doxygen/              Generated-reference entry/group declarations
```

Generated files and third-party downloads belong under `build`, never in source directories. Public headers use the `game_ex/...` include prefix. Source-only implementation details do not receive public headers.
