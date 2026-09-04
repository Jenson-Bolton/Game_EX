# Game_EX Engine

This project contains reusable, game-agnostic C++ engine modules. The first milestone implements only `GameEX::Core` and `GameEX::PlatformSDL`: a minimal application lifetime, a platform capability boundary, and an SDL3 desktop-window backend.

The engine must not contain transport simulation, Czech Republic source-data interpretation, editor workflows, or runtime knowledge of the world compiler. OpenGL and Vulkan will be isolated behind separate implementations of one shared Render API/RHI; SDL3 is not being used as a rendering abstraction.

See the workspace [engine section](../docs/sections/engine.md) and [architecture overview](../docs/architecture/overview.md).
