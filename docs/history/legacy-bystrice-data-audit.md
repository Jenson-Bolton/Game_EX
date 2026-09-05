# Legacy Bystřice data and provenance audit

| Field | Value |
| --- | --- |
| Audit date | 2026-09-05 |
| Audit type | Read-only recovery and acceptance review |
| Legacy project | IN3005 Route Runner / `Graphics_CW` |
| Intended Game_EX use | Evidence for the Bystřice proof slice and future parser tests |
| Acceptance result | **Rejected pending provenance repair** |
| Copy/commit result | **No legacy data artefact is approved for copying into or committing to Game_EX** |
| Combination result | **Prohibited until the acceptance evidence in this report is supplied** |

## Executive summary

This audit recovered the exact terrain and OpenStreetMap artefacts used by the earlier Bystřice coursework, inspected their binary and textual content, reconstructed the terrain conversion, and compared the available evidence with the [Game_EX data-source strategy](../project/data-source-strategy.md). The recovered files are useful engineering evidence, but they do not meet the project's provenance and licensing gate.

The strongest technical result is reproducible: the 89,892,709-byte `DMR5G.las` contains 2,996,407 points and was converted into the 512 by 384 `DMR5G_512x384.lgrid`. An in-memory reconstruction matched the shipped LGRID byte for byte. The source populated 153,005 cells; 43,603 cells, or 22.18%, were empty and were filled by 123 iterations of the converter's fallback simultaneous four-neighbour averaging algorithm. The resulting format discarded the original validity mask and contains no CRS, vertical datum, units, source identifier, licence, or parent checksum.

The legacy documentation claims that the terrain is ČÚZK / Land Survey Office DMR 5G acquired on 14 April 2026 under CC BY 4.0, but it also states that the exact ČÚZK download or order page still needed to be added. Neither the archive nor the LAS contains an authoritative product, CRS, vertical-reference, licence, or order record. The claim is therefore recorded as a **legacy project claim**, not as verified provenance.

The legacy LGRID is technically suitable for external, read-only parser and visualisation diagnostics because its bytes, layout, orientation, and derivation have been recovered. It is not an accepted DMR source or a redistributable project fixture. It must remain outside Game_EX, be labelled `rejected/diagnostic`, and must not be copied, committed, packaged, combined with accepted layers, or represented as canonical DMR data. The same decision applies to the recovered LAS, ZIP, OSM snapshots, and derived OSM files until their individual provenance and licensing records pass the current acceptance contract.

## Scope and questions

The audit addressed five questions:

1. Which exact legacy files supplied the Bystřice terrain and contextual map data?
2. What can be verified from those files and their source code without relying on the coursework narrative?
3. How was the LAS transformed into the custom LGRID, including grid orientation and empty-cell handling?
4. Which provenance, coordinate, licensing, and reproducibility facts remain unknown?
5. Can any legacy artefact enter Game_EX now, either as accepted world data or as a diagnostic fixture?

The work was deliberately read-only. No legacy file was changed, copied into Game_EX, downloaded again, or committed. No claim in the old report was promoted to a verified fact merely because it appeared in coursework documentation. Current provider terms were not checked on the network, so this report is a project acceptance decision rather than legal advice.

## Evidence classification and method

Statements in this report use the following evidence classes:

| Class | Meaning |
| --- | --- |
| **Verified artefact fact** | Measured directly from a file, archive, Git object, or deterministic reconstruction. |
| **Verified code behaviour** | Established by reading the exact legacy implementation and, where stated, reproducing its output. |
| **Legacy project claim** | Recorded by the coursework report or notes but not supported by an authoritative source record in the recovered package. |
| **Inference** | A bounded interpretation of verified observations; explicitly identified and never substituted for source metadata. |
| **Unknown** | Not recoverable from the inspected local evidence. |
| **Game_EX decision** | A conservative acceptance or workflow decision made under the current project policy. |

The method was:

- locate likely terrain, OSM, script, report, and Git-history evidence in the two local coursework trees;
- compute byte sizes and SHA-256 checksums, and compare duplicate artefacts;
- inspect the ZIP member table and LAS 1.4 header, variable-length records, point counts, classifications, extents, scales, and offsets;
- parse the LGRID header and every float32 sample;
- read the exact converter and runtime loader implementations;
- independently reconstruct grid assignment, aggregation, and empty-cell filling in memory, then compare the resulting bytes and SHA-256 checksum with the shipped LGRID;
- inspect the OSM JSON metadata, geometry, feature counts, derived CSVs, image metadata, and generation scripts;
- inspect the legacy repository commit that first introduced the terrain files; and
- assess the evidence against the mandatory inventory and stop-before-combination rules in the [data-source strategy](../project/data-source-strategy.md).

The audit did not attempt to infer a CRS from negative coordinate magnitudes, infer acquisition dates from LAS GPS time values, infer licence permission from a provider name, or reconstruct information that the derived files had discarded.

## Evidence locations

The principal legacy working tree inspected was:

`C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW`

An identical terrain source also exists below:

`C:\Uni\Year 3\CG\Comupter Graphics\CW\Graphics_CW`

The old Git repository is at:

`C:\Uni\Year 3\CG\Comupter Graphics\CW\Graphics_CW\.git`

The principal supporting paths were:

| Purpose | Exact legacy path |
| --- | --- |
| Terrain archive | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\DMR5G_2d03688f-382a-11f1-960b-005056904843.zip` |
| Extracted LAS | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\DMR5G_2d03688f-382a-11f1-960b-005056904843\DMR5G.las` |
| Derived LGRID | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\terrain\DMR5G_512x384.lgrid` |
| LAS converter | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\tools\convert_las_to_grid.py` |
| Runtime terrain loader | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\TerrainGrid.cpp` |
| Runtime terrain declaration | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\TerrainGrid.h` |
| Runtime integration | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\Game.cpp` |
| Railway integration | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\GameRailway.cpp` |
| Combined OSM snapshot | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\data\bystrice_osm.json` |
| Roads OSM snapshot | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\data\bystrice_osm_roads.json` |
| Land-cover generator | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\tools\generate_osm_landcover.py` |
| Scenery generator | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\tools\generate_osm_scenery.py` |
| Derived scenery | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\data\bystrice_osm_scenery.csv` |
| Derived land-cover image | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\textures\bystrice_osm_landcover.jpg` |
| Route data | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\terrain\osm_route.csv` |
| Track placement | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\OpenGLTemplate\resources\terrain\track_placement.txt` |
| Source notes | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\REPORT_SOURCES.md` |
| Asset notes | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\ASSET_DATA_SOURCES.md` |
| Submission notes | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\CW\Graphics_CW\RESIT_README.md` |
| Final report | `C:\Uni\Year 3\CG\Comupter Graphics - Copy\IN3005_220020891_RouteRunner_Submission\IN3005_220020891_Route_Runner_Final_Report.docx` |

These are external historical paths, not repository dependencies. Their inclusion records where the evidence was found; it does not authorise Game_EX to depend on those locations.

## Terrain source inventory

### Archive and LAS identity

| Artefact | Size | SHA-256 | Verified status |
| --- | ---: | --- | --- |
| `DMR5G_2d03688f-382a-11f1-960b-005056904843.zip` | 40,934,728 bytes | `C943B820CFB78640856AA5B52EB09C30E0B44955395487AACBD92E7E126FE439` | Exact local archive inspected |
| ZIP member `DMR5G.las` | 89,892,709 bytes uncompressed | `9F2C1132658EDD3731840B22340963A39FF09F98154ECA81EB93756FC3DC46C7` | Only member; matches extracted LAS |
| Extracted `DMR5G.las` | 89,892,709 bytes | `9F2C1132658EDD3731840B22340963A39FF09F98154ECA81EB93756FC3DC46C7` | Identical in both legacy coursework trees |
| `DMR5G_512x384.lgrid` | 786,496 bytes | `ED3DA3F399B40F033A4B860A81DEB76123425C6ED3E163E4B49E92DD1AE5D38A` | Exact derived grid inspected and reproduced |

The ZIP contains exactly one member, `DMR5G.las`, stored with DEFLATE compression. Its compressed member size is 40,934,612 bytes and its CRC32 is `93915EE8`. The archive has no comment, manifest, licence, or alternate data stream. Its member timestamp is 2026-04-14 19:48:36. That timestamp is a property of this archive entry, not proof of provider acquisition time.

The UUID-like suffix in the archive name is not established by any recovered authoritative record as an official ČÚZK product, order, or tile identifier. It must remain an opaque filename component.

### LAS header and point measurements

The following are **verified artefact facts** from the LAS 1.4 header and point data:

| Property | Value |
| --- | --- |
| LAS version | 1.4 |
| Point data format | 6 |
| Point record length | 30 bytes |
| Header size | 375 bytes |
| Point-data offset | 499 bytes |
| Point count | 2,996,407 |
| VLR count | 2 |
| EVLR count | 0 |
| X/Y/Z scale | `0.01`, `0.01`, `0.01` |
| X/Y/Z offset | `-520000`, `-1140000`, `0` |
| Minimum X | `-522509.66` |
| Maximum X | `-514347.76` |
| Minimum Y | `-1149397.30` |
| Maximum Y | `-1143431.81` |
| Minimum Z | `260.92` |
| Maximum Z | `735.59` |
| Horizontal envelope size | 8,161.90 by 5,965.49 coordinate units |
| Elevation range | 474.67 coordinate units |
| Classification | All 2,996,407 records have class value 8 |
| Classification flags | Synthetic, key-point, withheld, and overlap flags are all clear |
| System identifier | `LAStools (c) by rapidlasso GmbH` |
| Generating software | `lasmerge (version 241210)` |
| Header creation day/year | day 300 of 2025 |
| Project GUID | all zero |
| Global encoding | 17 |

The header contains no WKT or projection VLR. The only two VLRs use the user ID `ESRI_Export`, record IDs 5 and 3, descriptions `Local Time: 2026 04 14 19 48 18` and `Local Time: 2026 04 14 19 48 17`, and eight-byte binary payloads. They do not establish a horizontal CRS, axis order, vertical datum, or elevation units.

The raw GPS time range is `51007393.949897` to `52491532.384598`. No acquisition date is inferred from these values because the required time interpretation and acquisition metadata are absent.

The software identifier and `lasmerge` generator establish that this file passed through LAStools. They do not reveal the upstream input filenames, native tile identities, processing command, merge order, filtering, reprojection, or whether header metadata survived unchanged. The uniform class value also does not establish the semantic meaning of class 8 for this particular processed product.

## Derived LGRID format and conversion

### Binary layout

The exact custom LGRID layout is:

```text
offset  size                         field
0       4 bytes                      ASCII magic "LGRD"
4       uint32 little-endian         format version = 1
8       uint32 little-endian         width = 512
12      uint32 little-endian         height = 384
16      6 x float64 little-endian    minX, minY, maxX, maxY, minZ, maxZ
64      512 x 384 x float32          height samples in row-major order
```

The six stored bounds are the LAS bounds listed above. The file has no CRS identifier, axis declaration, horizontal or vertical units, vertical datum, source filename, parent checksum, licence, fill-policy identifier, no-data sentinel, or validity mask.

The grid measurements are:

| Property | Value |
| --- | ---: |
| Width | 512 columns |
| Height | 384 rows |
| Sample count | 196,608 |
| Column spacing | 15.972407045 coordinate units |
| Row spacing | 15.575691906 coordinate units |
| Minimum stored sample | 261.67181396484375 |
| Maximum stored sample | 734.180908203125 |
| Mean stored sample | 367.0453796386719 |
| NaN values | 0 |
| Infinite values | 0 |
| Zero values | 0 |
| Samples outside LAS header Z range | 0 |

Selected samples, indexed as `[row,column]`, are:

| Position | Value |
| --- | ---: |
| `[0,0]` | 293.0425 |
| `[0,511]` | 413.88235 |
| `[383,0]` | 263.5 |
| `[383,511]` | 370.919 |
| Central sample | 332.7374 |

The geographic-coordinate centre encoded by the bounds is X `-518428.71`, Y `-1146414.555`. The word “geographic” here means the source coordinate plane; it does not independently prove a CRS.

### Recovered grid algorithm

The exact converter is `convert_las_to_grid.py`, size 5,986 bytes, SHA-256 `4E91BCC2279E6823B22B30DB02AF722E69C448185501AF43022580E9EC946599`.

The converter performs the following operations:

1. Read each LAS X, Y, and Z signed integer directly from the point record.
2. Apply the header scale and offset to obtain coordinates.
3. Do not filter by classification, return number, flags, or point source ID.
4. Normalise X across `[minX,maxX]`, multiply by `width - 1`, and use nearest-integer rounding to select a column.
5. Normalise Y across `[minY,maxY]`, multiply by `height - 1`, and use nearest-integer rounding to select a row.
6. Accumulate Z and point count per cell with `bincount`, then store the arithmetic mean for populated cells.
7. Fill every empty cell. If SciPy is importable, the code requests a distance-transform nearest-populated-cell fill. Otherwise it repeatedly applies simultaneous four-neighbour averaging until no empty cell remains.
8. Write the fixed LGRID header followed by little-endian float32 heights in row-major order.

The recovered script contains two possible fill behaviours, but the shipped result identifies which branch actually produced it. Re-executing the fallback logic in memory produced:

| Reconstruction result | Value |
| --- | ---: |
| Initially populated cells | 153,005 |
| Initially empty cells | 43,603 |
| Empty proportion | 22.18% |
| Maximum points in one populated cell | 163 |
| Mean points per populated cell | 19.5837 |
| Four-neighbour propagation iterations | 123 |
| Reconstructed output hash | `ED3DA3F399B40F033A4B860A81DEB76123425C6ED3E163E4B49E92DD1AE5D38A` |
| Match with shipped LGRID | Exact, byte for byte |

This is **verified code behaviour plus deterministic reconstruction**: the shipped LGRID was produced by the fallback iterative four-neighbour averaging behaviour, not the SciPy nearest-cell behaviour. The reconstruction identifies the algorithm and output; it does not recover the exact historical command line, Python version, NumPy version, operating environment, or reason that the fallback branch was selected.

### Grid and runtime orientation

The converter and runtime agree on the following orientation:

- column 0 corresponds to `minX` and columns increase towards `maxX`;
- row 0 corresponds to `minY` and rows increase towards `maxY`;
- samples are stored row-major, so a row's columns are contiguous;
- there is no vertical row flip during conversion or runtime loading;
- runtime local X increases with source X and runtime local Z increases with source Y.

The legacy runtime centres the terrain envelope. Column 0 maps to local X `-4080.95`, and row 0 maps to local Z `-2982.745`. The terrain height used at runtime is:

```text
runtime_y = stored_absolute_height - 260.92
```

Both horizontal and vertical load scales are `1.0`. Sampling outside the terrain envelope is clamped at the edge by the legacy runtime.

### Information lost by conversion

The LGRID is not a lossless staged terrain product:

- 2,996,407 source points were reduced to 196,608 average/fill samples;
- point classification, return data, flags, source IDs, scan angles, GPS time, and point density were discarded;
- the 43,603-cell original void mask was discarded after interpolation;
- filled and observed cells cannot be distinguished from the LGRID alone;
- source precision was reduced to float32 sample heights;
- source identity, checksum, processing environment, CRS, units, vertical reference, and licence were not embedded.

Consequently, the legacy LGRID cannot be upgraded into a canonical accepted terrain package merely by wrapping it in a new Game_EX header. Acceptance requires returning to a provenance-complete raw source and regenerating the staged product with an explicit validity mask and build manifest.

## Legacy terrain provenance claims

The final coursework report and source notes contain the following claims:

| Field | Legacy project claim | Audit status |
| --- | --- | --- |
| Provider | ČÚZK / Land Survey Office | Not tied to the exact archive by authoritative local metadata |
| Product | DMR 5G | Filename and report agree, but no provider manifest identifies this exact file |
| Source location | `https://atom.cuzk.gov.cz/` | Generic service URL, not the exact download/order/product record |
| Acquisition date | 14/04/2026 | Reported date; not proved as provider acquisition time by the archive timestamp |
| Licence | CC BY 4.0 | Report claim only; no applicable licence text/URL or redistribution analysis accompanies the data |
| Horizontal CRS | EPSG:5514 | Consistent with the coursework transform and expected area; absent from LAS/LGRID metadata |
| Horizontal units | metres | Expected by the legacy code/report; absent from the source and derived file metadata |

`REPORT_SOURCES.md` explicitly records that the exact ČÚZK download or order page still needed to be added. That statement prevents the generic provider URL from being treated as complete provenance. The available evidence does not show which product edition or native tiles were merged, whether the source was downloaded directly from the claimed provider, or which terms applied to this specific acquisition.

The recovered coordinate bounds match the Bystřice proof envelope documented in the [data-source strategy](../project/data-source-strategy.md). This is a useful consistency check, not independent proof that the unlabelled LAS declares EPSG:5514. Under Game_EX policy, coordinate shape and location agreement cannot substitute for source-declared CRS, axis order, and vertical reference.

## OpenStreetMap source and derived artefacts

### Source snapshot identity

| Artefact | Size | SHA-256 | Verified content |
| --- | ---: | --- | --- |
| `bystrice_osm.json` | 6,011,969 bytes | `9813DE59D7B2DC31AF23862EA82A53DC7B5D2AA82D2B36B6F48A1B8DE2557640` | General Bystřice Overpass response |
| `bystrice_osm_roads.json` | 1,197,479 bytes | `8BADD7ADEE203A06374A87EE166EE66E4147F48A01C906486CE96141FF240B3D` | Highway-focused Overpass response |

The general snapshot records Overpass API generator `0.7.62.11 87bfad18`, OSM base timestamp `2026-08-10T23:38:53Z`, and an embedded ODbL notice. It contains 7,264 ways with complete geometry and 77,171 geometry points. Measured tag counts include 6,300 building ways, 75 railway-tagged ways, 62 ways tagged `rail`, 17 rail ways with reference 303, 190 waterway ways, 529 land-use ways, and 170 natural-feature ways.

The road snapshot records OSM base timestamp `2026-08-10T23:50:06Z`. It contains 1,715 highway ways and 15,743 geometry points. The two files therefore total 8,979 way records but are snapshots taken 11 minutes and 13 seconds apart, not one atomic database snapshot.

The geometry included in complete OSM ways exceeds the reported selection envelope:

| Snapshot | Latitude range | Longitude range |
| --- | --- | --- |
| General | `49.3265707` to `49.4440846` | `17.5160546` to `17.7740697` |
| Roads | `49.3488093` to `49.4317746` | `17.5900907` to `17.7747376` |

The coursework report records an intended Overpass bounding box of south `49.3652795`, west `17.6190867`, north `49.4255440`, east `17.7387194`. The exact Overpass QL queries and endpoint URL were not saved. Geometry outside the box is consistent with complete ways being returned, but the missing query prevents exact re-acquisition or verification of filters.

OSM way `215353459`, used as the TON boundary reference, is present in the general snapshot. Loukov station node `3313933869` is not present; its latitude `49.4103634` and longitude `17.7153493` are hard-coded in a generator from project notes. That hard-coded location may be useful as a reference check, but it is not traceable to the retained snapshot.

The JSON embeds the standard OpenStreetMap copyright and ODbL notice. The old notes identify the OpenStreetMap copyright page, ODbL 1.0, attribution guidance, and Overpass QL documentation. These are stronger local licensing indicators than the terrain package contains, but the Game_EX manifest still requires exact snapshot/query details, intended-use and redistribution analysis, and preservation of source attribution and identifiers before acceptance.

### OSM transformation behaviour

The relevant generator hashes are:

| Script | Size | SHA-256 |
| --- | ---: | --- |
| `generate_osm_landcover.py` | 5,358 bytes | `97DBD2FE3377A842F7B2492B5263CA511680068C1708ECB2EC623271A4BF1686` |
| `generate_osm_scenery.py` | 19,786 bytes | `00E28EA1FBA82B06762B007470518D6C6E6F0B44B81D25A48039076C830B2973` |

Both generators use `Transformer.from_crs(4326, 5514, always_xy=True)`. They subtract the centre of the recovered terrain envelope to obtain local positions. The installed `pyproj` and PROJ versions, transformation pipeline, grid resources, and reproducible environment were not recorded, so exact coordinate regeneration is not guaranteed by the scripts alone.

The scenery generator uses fixed per-class spacing and caps, and element-ID XOR-derived seeds. Its CSV output omits the source OSM identifiers, preventing direct record-level traceability after generation.

### Derived OSM products

| Artefact | Size | SHA-256 | Verified result and limitation |
| --- | ---: | --- | --- |
| `bystrice_osm_scenery.csv` | 3,748,494 bytes | `DA6ED0D9F308B147BD63D6C4BA5076921748EF3BDD60F489A4797D82710C1233` | 89,457 generated records; source OSM IDs discarded |
| `bystrice_osm_landcover.jpg` | 1,047,250 bytes | `721E264D8653842C7232831E27CE2CAFB04DC6461BBD1D069C8119409F1609AE` | 2048 by 1536 RGB JPEG; no EXIF or georeferencing; lossy and not reversible |
| `osm_route.csv` | 9,036 bytes | `7B732EF4FA86B708F514AAE8BFA6FAE9D7084D3D8B4FF1F494E86473286BABFC` | 248 rows; mixed OSM-derived and authored route, without source element IDs |
| `track_placement.txt` | 10 bytes | `0826E39D5AFF48C10CA08E39C3DDAEB728C94925FA9F4DE515B8648386447390` | Exact text `0 0 0 0.5` followed by newline; project-authored placement, not geographic source data |

The scenery CSV contains:

| Class | Record count |
| --- | ---: |
| Trees | 4,159 |
| Crops | 10,000 |
| Meadow | 65,000 |
| Grass | 2,470 |
| Shrub | 1,621 |
| Buildings | 6,202 |
| Platforms | 5 |
| **Total** | **89,457** |

One generated building centre has local X `4081.696`, which lies 0.746 coordinate units outside the nominal terrain maximum local X of `4080.95`. The old runtime hides this mismatch by clamping terrain sampling at the envelope. A future compiler must report or clip such records explicitly rather than rely on runtime clamping as an undocumented reconciliation rule.

The route CSV contains 117 rows labelled as the OSM Loukov-to-TON portion and 131 authored return rows. Its local range is X `-1205.812` to `2887.447` and Z `-367.301` to `1427.3`. Because it combines two origins and retains only a broad source string, it is not a smaller “safe” substitute for the raw OSM snapshots.

## Supporting documentation and code identity

These checksums anchor the exact legacy evidence used by the audit:

| Evidence | Size | SHA-256 |
| --- | ---: | --- |
| `REPORT_SOURCES.md` | 21,782 bytes | `B20870DC7DB029E1AD19BFDAFD16C27FC5D7283487BE6C62E61045D09AD60233` |
| `ASSET_DATA_SOURCES.md` | 5,413 bytes | `53821C37A73346345AC5CDDA5A8D9A5DAEF5C51B539C0103F3FDD0D349692198` |
| `RESIT_README.md` | 10,513 bytes | `7BBD33896FE3D2DB9846AED31D98653F6ED196A090CCA66AFCC5B82EDFF59086` |
| Final report DOCX | 1,228,196 bytes | `37DB3651CB0DAA2D07E328DDAA1751E8399E19A486FF9FF8CA3D6C452B1EF513` |
| `TerrainGrid.cpp` | 7,323 bytes | `958486933E2B190FD183FB86A709BD28BF4234A33D3FB518ABB0B1366CDB2664` |
| `TerrainGrid.h` | 880 bytes | `55C8C341F1ACF020DE83809FFFF757FBD8659320F5A2E67073DB7A419846D6D9` |
| `Game.cpp` | 22,965 bytes | `61265CA94824FAA7BA893B4789866D4223696229BE46F581A2422D20E3B46EE5` |
| `GameRailway.cpp` | 35,642 bytes | `C8449D905AF3D742956DA96CE320142CCD1C76BCF04BF8D856D0AAB8378EB5AA` |

The old Git repository remote is recorded as `https://github.com/Jenson-Bolton/Graphics_CW.git`. Commit `bb60f94f72cea91b89710baec9b247f1261ed673`, dated 2026-04-14 22:52:58 +01:00 with message `added terrain`, introduced both the ZIP and extracted LAS as ordinary Git blobs. Their blob IDs are `4a1b9855...` for the ZIP and `888dec7e...` for the LAS. This establishes when those bytes entered the old repository; it does not establish provider acquisition, licence, source identity, or permission to republish them.

## Provenance gap analysis

### Known and anchored

The following can be retained in a future provenance investigation because they are anchored to inspected bytes or exact legacy text:

- local archive, LAS, LGRID, OSM, script, derived-output, report, and code filenames;
- byte sizes and SHA-256 checksums recorded above;
- the ZIP membership, compression details, member CRC32, and archive-entry timestamp;
- numerical LAS version, layout, point count, scales, offsets, bounds, classifications, and generator strings;
- numerical LGRID format, bounds, dimensions, sample statistics, orientation, and byte order;
- the converter's point aggregation and both possible fill paths;
- the byte-for-byte reconstruction proving the fallback four-neighbour fill path for this output;
- the OSM snapshot timestamps, Overpass generator, embedded ODbL notice, measured feature counts, and geometry bounds;
- the generator transform request, local-origin rule, record counts, and derived-output hashes;
- the old repository commit that introduced the terrain bytes; and
- the exact wording of the legacy provider, product, date, generic URL, CRS, units, and licence claims.

### Unknown or insufficiently evidenced

The following mandatory fields remain unknown or insufficient:

- exact ČÚZK order, download, or stable product-record URL tied to the archive checksum;
- official order, dataset, delivery, product-edition, and native tile identifiers;
- provider-issued archive or member checksum and manifest;
- authoritative horizontal CRS, declared axis order, and horizontal units for this exact LAS;
- authoritative vertical datum/reference and elevation units;
- source epoch, acquisition date/time, acquisition method, operator, accuracy, density/resolution specification, and quality metadata;
- source coverage and no-data convention beyond the bounds measured from retained points;
- the upstream inputs, merge order, filters, command, and operator that produced the `lasmerge` output;
- applicable terrain licence text, version, URL, effective date, required attribution, derivative conditions, redistribution conditions, and the documented reason the intended Game_EX uses are permitted;
- whether raw terrain source bytes, a derived grid, or either checksum may be published in a public repository;
- exact historical converter command line and Python, NumPy, and optional SciPy versions;
- an original observed/empty validity mask for the LGRID;
- exact Overpass endpoint and full queries for both OSM snapshots;
- a single atomic OSM snapshot shared across the two query products;
- `pyproj`, PROJ, transformation pipeline, and transformation-grid versions/resources;
- preservation of OSM source IDs in the scenery, land-cover, and mixed route products;
- a complete OSM derivative/redistribution assessment for the intended packaged output; and
- independent validation records for coordinate round trips, expected references, terrain range, void handling, feature clipping, and tile seams.

## Acceptance decision

### Minimal recovered record

Until the missing evidence is supplied, the minimal Game_EX-facing record for every recovered terrain artefact is:

```yaml
region: Bystřice proof slice
legacy_project: IN3005 Route Runner / Graphics_CW
classification: rejected/diagnostic
acceptance: not accepted as canonical DMR input
copy_into_game_ex: prohibited
commit_or_redistribute: unresolved; not approved
combine_with_accepted_layers: prohibited
permitted_engineering_role: external read-only parser/visualisation diagnostic only
reason: source identity, authoritative CRS/vertical reference, licence,
  redistribution permission, and reproducible transformation manifest are incomplete
```

This is a **Game_EX policy decision**, not a finding that all private possession or inspection is unlawful. The evidence is insufficient to make an affirmative legal conclusion about copying or redistribution. The conservative project result is therefore that no legacy terrain or OSM data is accepted or copyable into Game_EX yet.

### Per-artefact decision

| Artefact | Technical diagnostic value | Accepted data status | Current project action |
| --- | --- | --- | --- |
| Terrain ZIP | Exact raw-package fingerprint and archive test input | Rejected | Keep external; do not copy, commit, package, or redistribute |
| LAS | Best future conversion source if provenance is repaired | Rejected | Keep external; do not call it canonical DMR input |
| LGRID | Deterministic parser/orientation/fill regression evidence | Rejected/diagnostic | May be opened in place for local read-only diagnostics; do not copy or ship |
| OSM JSON snapshots | Recoverable feature and geometry evidence | Rejected pending complete OSM manifest | Keep external and separate; do not combine |
| OSM scenery CSV | Reproducible counts but source IDs lost | Rejected | Do not use as accepted vector/scenery input |
| OSM land-cover JPEG | Visual comparison only; lossy and ungeoreferenced | Rejected | Do not treat as a spatially authoritative layer |
| Mixed route CSV | Small but provenance-stripped and partly authored | Rejected | Do not use as a “safe” replacement for raw OSM |
| Track placement text | Project-authored configuration, not world-source evidence | Unneeded for data acceptance | Recreate deliberately if the new design needs an equivalent |

The LGRID's diagnostic allowance is narrow. A local developer tool may read the external path to test rejection messages, byte parsing, bounds display, orientation, or visual comparison. The tool and report must visibly label the file as rejected and untrusted. The file must not be copied into a test-data directory, cached inside the repository, transformed into a committed derivative, silently accepted because parsing succeeded, or combined into a Game_EX world package.

## Evidence required to upgrade the terrain

The terrain can move from `rejected/diagnostic` to an accepted DMR input only when all of the following are present and mutually consistent:

1. **Authoritative source identity.** Recover the original ČÚZK order/delivery record or a stable provider product record tied to the exact downloaded package. Record provider, product, edition, order/dataset/tile identifiers, acquisition timestamp, responsible owner, original filename, byte size, and provider or project SHA-256 checksum.
2. **Licence and intended-use decision.** Retain the applicable licence text or stable URL, name/version/effective date, required attribution, derivative and share-alike obligations, redistribution conditions, and a written reason that local processing, generated derivatives, runtime packaging, public Git hosting, and distribution are each permitted or excluded. A generic service URL or unreferenced “CC BY 4.0” label is insufficient.
3. **Spatial and measurement metadata.** Obtain provider evidence for the exact horizontal CRS, axis order, horizontal units, vertical datum/reference, vertical units, epoch, coverage, resolution/density, accuracy, and no-data convention. Do not derive these fields from coordinate magnitudes.
4. **Processing lineage.** Identify every native source tile and checksum used by `lasmerge`, or reacquire an authoritative unmerged source. Record merge/filter/reprojection steps, their order, exact command/configuration, tool versions, grid resources, operator, and output checksum.
5. **Immutable acquisition manifest.** Store the approved metadata beside an external immutable cache record, including retention location, whether raw bytes may be committed, and why. A network response must never become an unversioned build input.
6. **Deterministic regeneration.** Convert the provenance-complete raw source with a versioned WorldCompiler adapter. Record adapter/script commit, command/configuration, compiler and dependency versions, classification/filter policy, grid geometry, rounding, interpolation/no-data policy, parent hashes, and output hash.
7. **Validity preservation.** Regenerate an observed/no-data validity mask from the raw points. The old LGRID cannot recover which 43,603 samples were filled, so it cannot satisfy this requirement by itself.
8. **Separate-layer validation.** Load the staged terrain independently in the world editor and record bounds, point/sample counts, expected height range, void/fill counts, coordinate round trips, reference-location alignment, rejection diagnostics, and visual evidence. Do not combine it with OSM or another source during this gate.
9. **Package verification.** Store CRS/vertical reference, double-precision tile origin, units, validity, source identity, parent checksums, transformations, and acceptance report in the resulting manifest. Reopen the package through the shared WorldFormat reader and verify deterministic checksum, range, orientation, and tile seams.

If the original package cannot be tied to authoritative identity and terms, the correct path is a new documented acquisition. The old hash remains useful for comparison but does not transfer acceptance to a replacement download.

## Evidence required to upgrade the OSM material

An accepted OSM layer additionally requires:

- immutable raw snapshot files with exact query text, endpoint, acquisition timestamp, base timestamp, filenames, sizes, and SHA-256 checksums;
- one declared snapshot policy, including how non-atomic query results are prevented or reconciled;
- recorded selection bounds and feature filters, with a policy for complete geometries extending outside the clip;
- retained OpenStreetMap element type, ID, version where available, and source tags through staging and derivation;
- `© OpenStreetMap contributors`, applicable ODbL terms, attribution placement, and a documented database/derivative/redistribution decision for the intended output;
- pinned transform implementation and resources from WGS 84 to the canonical Game_EX frame;
- explicit separation of OSM-derived route points from project-authored route points; and
- independent visual and numerical validation before any precedence or reconciliation step.

The retained JSON may help reconstruct some of this record, but the absent queries, split snapshot times, and provenance-stripped derivatives mean the current derived files cannot themselves satisfy the gate.

## Discussion and reflection

The legacy pipeline succeeded at producing a visually useful coursework terrain, but visual success and deterministic reverse engineering are not the same as data acceptance. The byte-for-byte LGRID reconstruction is strong evidence of how the output was generated. It is not evidence of who supplied the LAS, which formal coordinate and vertical systems apply, or whether redistribution is permitted.

The most consequential design loss is the erased validity mask. Almost one quarter of grid cells were not directly populated by a point, yet the final file represents observed averages and propagated values identically. A renderer can display the result, but a compiler cannot quantify confidence, revise the fill policy, or distinguish coverage from invention. This supports the Game_EX rule that staging and inspection precede irreversible packaging.

The LAS is a better future starting point than the LGRID because it retains point coordinates and attributes, but it remains a processed `lasmerge` output and lacks authoritative spatial metadata. Provenance repair must therefore establish its parent source, not merely improve the derived file's description.

The OSM work preserves more licensing text in the raw JSON than the terrain package does. However, derivation strips stable element IDs, the two snapshots are not atomic, and the exact queries are absent. Future tooling should treat attribution, IDs, query text, snapshot time, and transform configuration as first-class manifest data rather than report-only prose.

## Limitations and risks

- This was a local, read-only audit. No current ČÚZK, OSM, or other provider page was consulted, so provider terms may differ from the legacy claims or have changed.
- SHA-256 establishes byte identity, not authenticity, ownership, licence, or correctness.
- The LGRID reconstruction proves the algorithm-output relationship for the retained bytes; it does not prove the exact historical machine or command invocation.
- LAS bounds are measured facts, but interpreting them as EPSG:5514 metres remains a claim until authoritative metadata ties that CRS and unit to the exact source.
- The LAS may itself be a derivative of multiple native tiles, and no upstream inventory was recovered.
- The absence of a validity mask means some visual terrain detail is interpolation without retained confidence or coverage semantics.
- Runtime clamping can conceal out-of-bounds derived OSM content and must not be reused as silent compiler validation.
- Local availability of a file does not establish permission to copy it into a new public repository.
- This report records a technical governance decision and does not replace legal review where licence interpretation is material.

## Conclusion

The audit establishes exact identities and useful technical behaviour for the legacy Bystřice inputs, especially the terrain conversion, but it also confirms that the recovered package does not meet Game_EX's acceptance contract. No legacy file is currently accepted, approved for copying into the repository, approved for redistribution, or eligible for combination.

The preferred recovery path is to obtain or reacquire a provenance-complete authoritative DMR source, preserve the original raw snapshot externally, and compile it through the new manifest-driven WorldCompiler. The legacy LGRID should remain an external rejected/diagnostic comparison sample only. Its value is as evidence of expected parsing, orientation, bounds, and historical visual output—not as the foundation of the new world dataset.

## Traceability

- [Game_EX data-source strategy](../project/data-source-strategy.md)
- [Version-history policy](README.md)
- [Technical report template](report-template.md)
- Legacy repository commit introducing terrain: `bb60f94f72cea91b89710baec9b247f1261ed673`
- Terrain ZIP SHA-256: `C943B820CFB78640856AA5B52EB09C30E0B44955395487AACBD92E7E126FE439`
- Terrain LAS SHA-256: `9F2C1132658EDD3731840B22340963A39FF09F98154ECA81EB93756FC3DC46C7`
- Derived LGRID SHA-256: `ED3DA3F399B40F033A4B860A81DEB76123425C6ED3E163E4B49E92DD1AE5D38A`
