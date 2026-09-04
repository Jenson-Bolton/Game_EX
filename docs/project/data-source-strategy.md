# Czech Republic data-source strategy

This document records the intended source hierarchy, spatial conventions, and evidence workflow for the first real-world Game_EX data path. It is a strategy and acceptance contract, not permission to download or redistribute a dataset whose current licence, product edition, or provenance has not been verified.

The hierarchy was recovered from the earlier TESSERA and Bystřice discussions. Earlier coursework artefacts may be used as evidence when designing tests, but they are not silently copied into Game_EX. Every source used here must enter through a new, explicit provenance record.

## Source roles

No source is authoritative for every property. Precedence is evaluated per domain claim—terrain height, building identity, road geometry, agricultural use, or inferred surface character—rather than by applying one dataset-wide ranking.

| Role | Preferred source | Intended use | Constraint |
| --- | --- | --- | --- |
| Bare-earth terrain | DMR 5G, with DMR 4G as an explicit fallback | Canonical ground elevation and terrain derivatives | DMR 5G wins where valid coverage exists. A DMR 4G fallback remains labelled and must not be blended across a seam without a recorded rule and seam test. |
| Surface elevation | DMP | Surface-height evidence for buildings, vegetation, and other above-ground features | DMP does not replace DMR terrain. Any derived height such as `DMP - DMR` requires compatible epochs, grids, horizontal coordinates, and vertical references. |
| Registered buildings, addresses, territorial units, and parcel references | RÚIAN, primarily through VFR; relevant INSPIRE/CPX products where their defined content is required | Stable identities and official attributes, plus authoritative boundaries where the selected product actually defines them | A definition point or identifier is not a footprint. VFR, INSPIRE, and CPX representations of the same underlying entity are alternatives or complements selected in the manifest, not independent votes to ingest twice. |
| General topography | ZABAGED | Hydrography, transport, land-cover boundaries, and other national topographic features within the product specification | Its defined feature geometry overrides supplemental or inferred geometry for the same claim. |
| Detailed mapped infrastructure | DMVS | Applicable detailed transport and technical-infrastructure geometry | Precedence against ZABAGED is decided by feature domain, product date, accuracy, and identifier—not by a blanket “newest file wins” rule. |
| Agricultural parcels and declared agricultural use | LPIS | Agricultural boundaries, declared use, and weak labels for semantic interpretation | LPIS overrides machine-learned land-cover geometry within its valid scope. Its administrative meaning must not be broadened into a general land-cover truth. |
| Community mapping | OpenStreetMap | Supplemental connectivity, names, access detail, and gap filling | OSM never overwrites an applicable authoritative identity or geometry silently. Every retained OSM value keeps source attribution and snapshot metadata. |
| Inferred land cover | Dynamic World | A dated probabilistic land-cover signal, comparison layer, and possible weak-label input | It is machine-learned evidence rather than cadastral or topographic authority. Confidence and observation date remain attached. |
| Learned surface semantics | TESSERA 2024 | Offline embeddings from which a versioned decoder may derive classes, probabilities, or material weights | TESSERA embeddings are not terrain, vector geometry, or a ready-made 3D world. They are used offline under weak supervision and are not shipped as raw runtime inputs. |
| Visual validation | Orthophoto appropriate to the selected official product and epoch | Human inspection of alignment, omissions, and obvious classification errors | Validation only. It is not traced, trained on, committed, or redistributed unless a later licence review explicitly permits that use. |
| Later statistics and public transport | CZSO statistics and NeTEx feeds | Population/economic calibration and scheduled public-transport data after the spatial proof | These are later domain integrations with separate temporal, licensing, and semantic decisions; they do not block the first spatial proof. |

## Precedence and conflict policy

The compiler applies these rules in order:

1. Reject a source whose identity, licence, snapshot date, coordinate reference system, units, or required provenance is missing. Unusable evidence has no precedence.
2. Use the source designated for the property being resolved. RÚIAN governs its registered entities, ZABAGED its specified topographic features, DMVS its applicable detailed infrastructure, and LPIS its agricultural domain.
3. Prefer DMR 5G for bare-earth elevation. Use DMR 4G only as a declared coverage fallback. Treat DMP as surface evidence and never as replacement ground terrain.
4. Preserve authoritative vector identity and geometry over OpenStreetMap, Dynamic World, or TESSERA-derived output wherever their claims overlap.
5. Use OpenStreetMap only to add a missing supplemental property or feature under a documented matching rule. An OSM feature that disagrees with authoritative data is retained as a conflict candidate or diagnostic, not used as a silent overwrite.
6. Use LPIS and other applicable authoritative polygons before machine-learned classes for agricultural areas. Dynamic World labels and a TESSERA 2024 decoder may fill unclassified space, provide confidence, or flag change; they must not move an authoritative boundary.
7. Treat disagreement between machine-learned sources as uncertainty. Preserve each prediction, confidence, model/decoder version, and observation period until an explicit reconciliation rule resolves it.
8. Escalate unresolved authoritative conflicts as compiler diagnostics. Preserve both source references; never resolve them through input order, filesystem order, or an undocumented “last value wins” rule.

TESSERA weak supervision is an offline, reproducible operation. Training or calibration labels may come from spatially and temporally compatible RÚIAN, ZABAGED, DMVS, LPIS, or Dynamic World evidence, but label origin and confidence remain distinguishable. Training, validation, and geographically separated hold-out areas must be recorded. Raw 128-dimensional embeddings are not processed repeatedly by the game renderer.

## Canonical coordinates and precision

The compiler's canonical horizontal coordinate reference system is EPSG:5514. Canonical positions, source bounds, tile origins, and reconciliation calculations use double-precision metres. The manifest records the input CRS, declared axis order, units, transform implementation, and any required grid resource; the compiler must not infer an axis swap from the magnitude or sign of coordinates.

EPSG:5514 defines the horizontal frame only. Terrain and surface sources also record their vertical reference, units, and no-data convention. Layers with unknown or incompatible vertical references are inspected separately and cannot participate in height subtraction or combined elevation output.

Runtime and rendering data uses tile-local single-precision coordinates. For each component, the conversion is conceptually:

```text
tile_local_float = float(canonical_double - tile_origin_double)
```

The tile origin stays in double precision, and national-scale EPSG:5514 values are never cast directly to float. Tile boundaries, origin selection, rounding policy, and seam checks must be deterministic. Coordinate transformation occurs offline; each normalised feature retains its source identifier and enough provenance to trace it back to the unmodified snapshot.

This strategy does not select a serialisation syntax, binary world layout, or transformation library. Those remain specification decisions.

## Inspect-before-combine workflow

World compilation proceeds through explicit stages:

1. **Acquire.** Place an immutable source snapshot in an external data cache. Never make an unversioned network response part of a build.
2. **Inventory.** Record provenance, licence, checksum, coverage, time, CRS, units, resolution, and redistribution status before parsing.
3. **Stage.** Read one source through a source-specific adapter and report malformed records, unsupported constructs, and rejected values.
4. **Normalise.** Transform accepted records into the EPSG:5514 double-precision canonical model without merging identities or semantics.
5. **Inspect separately.** Export or expose each normalised layer independently. The world editor must load it through shared engine/world contracts, display its bounds and counts, and allow the layer to be viewed without any combined output.
6. **Validate.** Check coordinate round trips, expected extents, no-data handling, terrain ranges, identifiers, topology, duplicate candidates, and tile seams. Orthophoto may support a labelled visual check only.
7. **Reconcile.** Apply the domain precedence rules above and emit a machine-readable conflict/decision report. No layer is combined merely because it was loaded earlier.
8. **Package.** Tile and write deterministic runtime data only after separate-layer acceptance. Record every input and transformation checksum in the build manifest.
9. **Reopen.** Load the produced package through the same WorldFormat reader used by the game and editor, then compare it with the accepted separate layers.

Acquisition, staging, normalisation, reconciliation, and packaging must be independently repeatable. A changed input snapshot, decoder, transformation grid, configuration, or tool version creates a different documented build.

## Bystřice proof slice

The first proof region is the area around Bystřice pod Hostýnem, Loukov railway station, and the TON site in the Czech Republic. The recovered terrain envelope is:

| Coordinate representation | Minimum | Maximum |
| --- | ---: | ---: |
| EPSG:5514 X (metres) | `-522509.66` | `-514347.76` |
| EPSG:5514 Y (metres) | `-1149397.30` | `-1143431.81` |
| WGS 84 longitude (reference check only) | `17.6190867` | `17.7387194` |
| WGS 84 latitude (reference check only) | `49.3652795` | `49.4255440` |

These bounds identify the proof and provide a transformation sanity check; they do not establish the CRS of an unlabelled file. Each selected source must declare its own coverage, and clipping must be recorded as a transformation.

The proof advances one independently visible layer at a time: terrain, authoritative vectors, supplemental OSM, agricultural evidence, and then Dynamic World/TESSERA-derived semantics. DMR 4G may substitute for missing DMR 5G only when the report says so. A source is accepted only when its layer can be loaded separately, its expected extent and key counts are reported, and known reference locations align. The first combined output must also include conflict counts, rejected-record counts, tile-seam results, and a deterministic package checksum.

The proof is not evidence that the pipeline scales to the whole Czech Republic. National ingestion, temporal updates, CZSO integration, and NeTEx integration require later representative tests and their own reports.

## Provenance and licensing record

Every raw, sampled, transformed, generated, or project-authored input has a manifest record containing at least:

- provider, product and edition, stable source location, acquisition timestamp, and responsible project owner;
- licence name/version and URL, required attribution, redistribution conditions, derivative/share-alike obligations, and the recorded reason the intended use is permitted;
- immutable source filename, byte size, cryptographic checksum, archive members used, and whether the source itself may be committed;
- geographic bounds, temporal coverage or epoch, CRS identifier, declared axis order, horizontal and vertical units, vertical reference, resolution/scale, accuracy metadata, and no-data convention;
- selection query, feature filters, clip bounds, and any source identifiers needed to reproduce the subset;
- adapter, transformation tool, grid resources, decoder/model, configuration, random seed, and exact versions or commits;
- parent input checksums, ordered transformation steps, validation results, output checksum, and classification as raw, staged, normalised, reconciled, sampled, generated, or project-authored;
- retention location, exclusion rationale for large or restricted inputs, and the report/version that accepted the result.

OpenStreetMap-derived data must carry `© OpenStreetMap contributors`, the applicable ODbL terms, snapshot/query details, and any resulting database obligations. Official Czech Republic products must be checked individually; a common publisher does not imply a common licence. Dynamic World and TESSERA terms must be reviewed for the exact product and use, including derived labels or trained decoders. Orthophoto remains outside committed or generated assets unless its exact terms permit the proposed operation and redistribution.

If a licence, attribution, coordinate convention, epoch, or semantic meaning remains uncertain, the layer remains separate and the compiler stops before combination.
