# Game_EX Engine

This project contains reusable, game-agnostic C++ engine modules. The current foundation implements `GameEX::Jobs`, `GameEX::Startup`, `GameEX::Core`, `GameEX::Platform`, `GameEX::PlatformSDL`, `GameEX::Render`, `GameEX::RenderOpenGL`, and `GameEX::RenderVulkan`: bounded worker batches, deterministic lifecycle orchestration, a minimal rendered application lifetime, platform-neutral windows, an SDL3 backend, a shared diagnostic Render API, and verified OpenGL 4.6 Core and Vulkan 1.3 clear/present paths.

The engine must not contain transport simulation, Czech Republic source-data interpretation, editor workflows, or runtime knowledge of the world compiler. OpenGL and Vulkan are isolated behind private backend targets while their public factories implement the same backend-neutral Render contract. SDL3 supplies native windows, OpenGL contexts, and Vulkan surfaces without leaking SDL or Vulkan types through common public headers. The Vulkan slice is deliberately transfer-clear only, so this release exposes no shaders, meshes, world data, or speculative graphics-resource API.

See the workspace [engine section](../docs/sections/engine.md) and [architecture overview](../docs/architecture/overview.md).
