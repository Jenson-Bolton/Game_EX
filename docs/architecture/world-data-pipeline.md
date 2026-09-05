# World data pipeline

The intended high-level pipeline is offline-first:

```text
terrain + authoritative vectors + buildings + transport + statistics
                              + semantic sources such as TESSERA
                                      |
                                      v
                       canonical spatial world model
                                      |
                                      v
                      versioned, streamable world packages
                                      |
                                      v
                           game/editor WorldFormat reader
```

TESSERA supplies learned annual surface/land characteristics, not terrain elevation, precise railway geometry, building footprints, or a 3D model. Its embeddings should be decoded offline into compact semantic classes, probabilities, or material weights, then reconciled with authoritative vector and terrain sources. Raw 128-dimensional cells should not be processed repeatedly in the runtime renderer.

The implemented portable slice begins after source-specific normalisation. A strict `.gexstage` manifest binds one terrain tile to its text height/validity files by exact byte size and SHA-256, preserves source and transformation provenance, and fixes canonical EPSG:5514 axes/units. WorldCompiler then emits a deterministic `.gexworld` package and `GameEX::WorldFormat` verifies and decodes it. The standalone editor can now map the decoded height/validity values to a bounded aspect-correct plan view through OpenGL or Vulkan. It does not yet acquire a source, transform coordinates, parse LAS/LAZ, reconcile layers, or process TESSERA.

The first real proof will use one small, named region and a minimal set of layers before any whole-Czech-Republic build. Dataset licences, dates, coordinate systems, accuracy, and transformations must be recorded alongside produced assets. Each layer must first be opened and visualised alone; package reconciliation begins only after that inspection succeeds. The current `64 x 64` display reduction may confirm broad structure and missing data but is not full-resolution or quantitative evidence.

The detailed source roles, conflict precedence, EPSG:5514 precision policy, Bystřice proof extent, and provenance fields are defined in the [Czech Republic data-source strategy](../project/data-source-strategy.md).

The [world package format](world-package-format.md) defines the current staging and runtime bytes. The [legacy Bystřice audit](../history/legacy-bystrice-data-audit.md) explains why the coursework LAS/LGRID cannot be accepted as the real proof input despite being technically recoverable.
