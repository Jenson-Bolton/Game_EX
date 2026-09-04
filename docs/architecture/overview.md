# Architecture overview

The source tree is divided by dependency and responsibility, not by a generic collection of “managers.”

```text
game_ex -----------------------+
                               |
game_ex_world_editor ----------+--> GameEX::GameApp
                                      |       |
                                      |       +--> GameEX::WorldFormat
                                      |
                                      +--> GameEX::Core
                                      +--> GameEX::PlatformSDL --> SDL3

game_ex_world_compiler ------------> GameEX::WorldFormat
```

`GameEX::Core` sees only the abstract `Platform` and `Window` contracts. `GameEX::PlatformSDL` implements those contracts and is the only current target that includes SDL headers. The game and editor composition roots select SDL3, then transfer ownership into `Application`.

`GameEX::WorldFormat` is deliberately tiny. It establishes a compiler/runtime boundary without pretending that an on-disk schema has been designed. The logical header must not be serialized by copying its C++ memory representation.

The next engine shape is expected to add independent targets for jobs, startup, resources, serialization, ECS, world runtime, input, audio, render abstraction, RHI, and a Vulkan backend. Those are directions, not permission to scaffold unused directories or placeholder abstractions.

## Dependency rules

- Engine modules never depend on game, editor, or Czech Republic domain code.
- Game and editor never include SDL, Vulkan, or native operating-system headers.
- Vulkan dependencies will be private to a Vulkan RHI target.
- World compiler implementation is offline; only WorldFormat is a runtime dependency.
- Platform exposes low-level capabilities. Higher layers own assets, saves, and domain workflows.
- Ownership is explicit and lifetimes are deterministic; service-locator singletons are prohibited.
