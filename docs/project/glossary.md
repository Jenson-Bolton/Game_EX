# Glossary

**Composition root** — The small executable layer that selects and owns concrete implementations.

**DMR** — Digital terrain model data used to describe ground elevation.

**DMR 5G** — The preferred CUZK fifth-generation digital terrain model for the first bare-earth terrain proof; each delivery still requires product-specific provenance and licence acceptance.

**DMP** — Digital surface model used as separate above-ground surface evidence, not as a replacement for DMR terrain.

**ECS** — Entity Component System; one possible representation for appropriate runtime objects, not the universal world database.

**RHI** — Render Hardware Interface, the engine boundary above concrete backends such as OpenGL and Vulkan.

**SDL3** — Cross-platform library used here only for the first platform/window backend.

**Staged world (`.gexstage`)** — A strict line-oriented manifest plus separately hashed normalised terrain/validity files. It is compiler input, not a runtime package or raw-source format.

**Startup DAG** — Directed acyclic dependency graph used to validate, schedule, profile, and reverse the lifecycle of future subsystems.

**TESSERA** — Annual Earth-observation embeddings intended for offline semantic derivation, not raw runtime rendering.

**WorldFormat** — Runtime-readable contract shared between the offline world compiler and game/editor consumers.

**World package (`.gexworld`)** — The deterministic, integrity-checked binary output consumed through WorldFormat. Version 0.2 currently carries exactly one terrain tile and its provenance.

**World compiler** — Offline program that validates normalised stages and packages runtime world tiles; future source-specific tooling will also acquire, transform, and reconcile accepted datasets.

**World editor** — User-facing authoring/inspection application. Its precise workflow and UI remain to be specified.
