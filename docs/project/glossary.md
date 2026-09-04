# Glossary

**Composition root** — The small executable layer that selects and owns concrete implementations.

**DMR** — Digital terrain model data used to describe ground elevation.

**ECS** — Entity Component System; one possible representation for appropriate runtime objects, not the universal world database.

**RHI** — Render Hardware Interface, the engine boundary above concrete backends such as OpenGL and Vulkan.

**SDL3** — Cross-platform library used here only for the first platform/window backend.

**Startup DAG** — Directed acyclic dependency graph used to validate, schedule, profile, and reverse the lifecycle of future subsystems.

**TESSERA** — Annual Earth-observation embeddings intended for offline semantic derivation, not raw runtime rendering.

**WorldFormat** — Runtime-readable contract shared between the offline world compiler and game/editor consumers.

**World compiler** — Offline program that will reconcile source datasets and package runtime world tiles.

**World editor** — User-facing authoring/inspection application. Its precise workflow and UI remain to be specified.
