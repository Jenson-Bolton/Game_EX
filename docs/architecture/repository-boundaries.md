# Repository boundaries

The architecture discussion selected three independent project boundaries and
anticipated that they may become separate repositories with a lightweight local
workspace:

```text
Game_EX workspace
|-- Engine
|-- WorldCompiler
`-- Game
```

For the foundation milestone they live together in the existing `Jenson-Bolton/Game_EX` repository. Each directory has its own top-level `project()` declaration and can be configured independently where its dependencies are available. The root CMake project is only the convenient composition build.

The current target graph also preserves the intended runtime/offline boundary.
`GameEX::GameApp` depends only on Engine targets (`GameEX::Render`,
`GameEX::Core`, `GameEX::PlatformSDL`, and the selected private renderer
backends). `GameEX::WorldEditorApp` adds
`GameEX::WorldFormat` so the editor can open and visualise a compiled package,
but it does not link `GameEX::WorldCompilerCore`. Compilation remains an offline
tool operation rather than an editor or game dependency.

This temporary monorepository arrangement preserves the existing GitHub history and avoids inventing three remote repository names, access policies, versioning schemes, and release workflows without approval. The Game standalone build currently finds sibling source projects; installed CMake package discovery should replace that bridge when repositories are split.

Before splitting, specify:

- remote repository names and public/private visibility;
- semantic versioning and compatibility promises;
- whether the workspace tracks dependencies as Git submodules or package versions;
- package export/install layouts for Engine and WorldFormat;
- coordinated CI and release rules;
- ownership of shared documentation and issue tracking.

No nested Git repository should be created inside this workspace as a shortcut. Any future multi-repository composition must follow the accepted split/migration and workspace decisions.

[ADR 0005](../decisions/0005-world-compiler-repository-split-gate.md) keeps portable WorldCompiler work in this repository but requires a new split decision before specialised geospatial, Python, acquisition, or machine-learning dependencies enter the supported build.
