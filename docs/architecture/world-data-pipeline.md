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

The compiler will eventually handle coordinate-reference conversion, source provenance, tiling, validation, reproducibility, change over time, confidence, and package versioning. None of those policies is safe to infer from the current window milestone.

The first proof should use one small, named region and a minimal set of layers before any whole-Czech-Republic build. Dataset licences, dates, coordinate systems, accuracy, and transformations must be recorded alongside produced assets.

The detailed source roles, conflict precedence, EPSG:5514 precision policy, Bystřice proof extent, and provenance fields are defined in the [Czech Republic data-source strategy](../project/data-source-strategy.md).
