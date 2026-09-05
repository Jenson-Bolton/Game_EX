# Game_EX Engine

This project contains reusable, game-agnostic C++ engine modules. The current foundation implements `GameEX::Jobs`, `GameEX::Startup`, `GameEX::Core`, `GameEX::Platform`, `GameEX::PlatformSDL`, `GameEX::Render`, and `GameEX::RenderOpenGL`: bounded worker batches, deterministic lifecycle orchestration, a minimal rendered application lifetime, platform-neutral windows, an SDL3 backend, a shared diagnostic Render API, and a verified OpenGL 4.6 Core clear/present path.

The engine must not contain transport simulation, Czech Republic source-data interpretation, editor workflows, or runtime knowledge of the world compiler. OpenGL is isolated behind its own backend, Vulkan will follow the same shared API boundary, and SDL3 is used for platform/context presentation integration rather than as the rendering abstraction. This release intentionally exposes no shaders, meshes, world data, or speculative graphics-resource API.

See the workspace [engine section](../docs/sections/engine.md) and [architecture overview](../docs/architecture/overview.md).
