# Game_EX Engine

This project contains reusable, game-agnostic C++ engine modules. The current foundation implements `GameEX::Jobs`, `GameEX::Startup`, `GameEX::Core`, and `GameEX::PlatformSDL`: bounded worker batches, validated serial and controlled-parallel subsystem lifecycles, a minimal application lifetime, a platform capability boundary, and an SDL3 desktop-window backend.

The engine must not contain transport simulation, Czech Republic source-data interpretation, editor workflows, or runtime knowledge of the world compiler. OpenGL and Vulkan will be isolated behind separate implementations of one shared Render API/RHI; SDL3 is not being used as a rendering abstraction.

See the workspace [engine section](../docs/sections/engine.md) and [architecture overview](../docs/architecture/overview.md).
