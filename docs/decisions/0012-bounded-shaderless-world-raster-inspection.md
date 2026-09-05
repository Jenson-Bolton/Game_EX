# ADR 0012: bounded shaderless world-raster inspection

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

The first portable `.gexworld` package contains one row-major terrain tile with
height offsets, validity, EPSG:5514 placement, spacing, and provenance. The
runtime-only `GameEX::WorldFormat` reader can verify and decode it, while the
world editor from `v0.1.6` opens a real window through either OpenGL 4.6 Core or
Vulkan 1.3. The two paths are not yet connected.

The next proof must show that the editor loads package data before rendering and
that both real backends display the same derived information. It must not link
the offline compiler into either application, invent a general render-hardware
interface from one 2 x 2 fixture, or silently turn the requested simple viewer
into an unspecified 3D editor.

A scalable texture path would immediately select shader languages and build
tools, SPIR-V publication, pipeline and descriptor layouts, texture ownership,
staging, sampling, reflection, and deployment policy. Those decisions will be
necessary for detailed real layers and interactive views, but the synthetic
fixture does not yet provide evidence for their final shape.

## Decision

Treat "simple visualisation" in this slice as an aspect-correct 2D plan view.
The editor may still open its diagnostic shell without data. With exactly one
`--world=<path.gexworld>` option, it reads and validates the complete package
before creating SDL, a native window, or a renderer. Package I/O, version,
integrity, and mapping failures therefore cannot trigger graphics fallback or
briefly show a misleading window.

Editor-only code owns package interpretation and colour mapping. It computes
global minimum and maximum from valid height samples, maps valid values through
a fixed blue-green-yellow linear-colour ramp, and marks invalid data in vivid
magenta. A constant valid field uses the ramp midpoint. Row zero is the lower
display edge: columns advance right and the package's positive Y direction
advances upward. The log retains the package identity, checksum, dimensions,
validity, height range, CRS, spacing, classification, provider, edition,
attribution, display reduction, and mapping semantics.

The backend-neutral Render contract grows only with an owning diagnostic raster:

- a linear-RGBA background;
- optional raster columns and rows;
- a finite positive display aspect ratio;
- one row-major linear-RGBA colour per cell.

The raster is bounded to 64 columns, 64 rows, and 4,096 cells. Dimensions must
both be non-zero when a raster exists, the colour count must be exact, and all
colour/aspect values must be finite and within their documented ranges. Owning
storage survives the hidden presentation attempt, visible loop, and a fresh
automatic renderer attempt without borrowed-span lifetime risk. Invalid public
input is rejected before native frame work and remains retryable.

A shared private integer layout policy fits the declared aspect into the current
drawable with centred letterboxing. Sixty-four-bit intermediates partition the
content rectangle into complete, non-overlapping cells even at odd pixel sizes.
It expresses row zero from the lower edge; each backend performs only the native
coordinate conversion it requires.

The displayed aspect is the regular grid's half-cell-padded sample footprint:
`columns * spacing_x` by `rows * spacing_y`. In the conceptual unreduced grid,
each source sample owns a cell centred on it and the outer cells extend half a
spacing beyond the outer sample centres. Reduced display cells summarize source
partitions while retaining that full footprint. This is deliberately distinct
from WorldFormat's numeric first-to-last sample-centre bounds,
`(dimension - 1) * spacing`, and does not redefine those coordinates.

OpenGL first clears the complete sRGB framebuffer to the background, then uses
`GL_SCISSOR_TEST` and one scissored clear per logical cell; a zero-area cell is
a defined no-op. It disables the scissor before presentation. Vulkan requires and enables the Vulkan 1.3
`dynamicRendering` feature in addition to `synchronization2`, requires surface
colour-attachment usage and compatible format features, and owns one image view
per swapchain image. A dynamic-rendering load clear writes the background and
`vkCmdClearAttachments` writes the calculated cell rectangles before the image
returns to presentation layout. Image views are part of the transactional
swapchain generation and are destroyed before their swapchain.

The player-facing game keeps the foundation frame and must not link editor-only
package/mapping code. The editor implementation is a separate CMake target that
links `GameEX::WorldFormat` and the shared desktop/rendering code but never
`GameEX::WorldCompilerCore`. The world compiler remains an offline executable.

The project-authored synthetic staging fixture is compiled into a test-owned
package by a CTest setup fixture. Explicit OpenGL and Vulkan editor smokes open
that package. Pure tests cover package-to-colour mapping, orientation, flat and
all-invalid inputs, deterministic reduction, aspect calculation, argument
validation, and pre-window failure. The OpenGL renderer smoke exercises a
3 x 2 raster and the Vulkan smoke a 2 x 2 raster;
required Vulkan validation still performs repeated presentation and rejects any
error-severity message.

For a source larger than 64 x 64, the editor derives a display-only overview. It
uses floor-divided, complete, non-overlapping source bins; averages valid heights
in double precision; retains global source-height normalisation; marks empty bins
invalid; and blends partially valid bins toward the invalid colour according to
their invalid fraction. Source package bytes and values are never rewritten.
The log distinguishes source and displayed dimensions and names the aggregation.

## Consequences

The existing synthetic package becomes visibly inspectable through both graphics
technologies with no shader compiler, runtime shader file, SPIR-V binary, vertex
buffer, texture, sampler, descriptor set, or pipeline. The same raster-shaped
public input can later feed an internal texture/full-screen draw, so editor
mapping and application composition do not need to change when the backend
implementation scales.

The bounded viewer is deliberately diagnostic. At most 4,096 per-cell clears are
recorded each frame. It is suitable for confirming broad terrain shape,
orientation, missing-data distribution, source identity, and the first reduced
DMR overview. It is not evidence for exact point inspection, quantitative cursor
queries, multiple semantic overlays, interactive pan/zoom, or production terrain
rendering.

A scalable upload and draw path becomes mandatory before full-resolution or
multiple rasters, interactive navigation, TESSERA overlay comparison, or general
editor viewport work. That later decision must specify shader source/build and
embedding, binding/reflection policy, resource ownership and update identity,
sampling, capture colour space, and viewport/camera interaction. A navigable 3D
terrain view additionally requires geometry, depth, camera, and precision
contracts and remains a project-owner specification gate.

The WorldFormat schema and WorldCompiler dependencies do not change, so this
slice does not cross the repository split gate in ADR 0005.

## Alternatives considered

Uploading a raster texture and drawing a full-screen primitive would scale
better, but commits the project to an entire shader/resource toolchain before
the first data-to-window connection is proven. Generating terrain geometry would
also force a 3D camera and depth policy that was not requested. Rendering the
package only in one backend would violate the cross-technology requirement.
Putting package knowledge in Engine would contaminate reusable rendering with
EPSG, terrain, and provenance concepts. Linking editor helpers into the game
would weaken the executable boundary. Loading after a window opens would allow
data failures to masquerade as renderer attempts. Stretching the grid to the
window would make spatial shape misleading.

## References

- [Vulkan 1.3 feature structure](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan13Features.html)
- [Vulkan dynamic rendering command](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBeginRendering.html)
- [Vulkan clear-command semantics](https://docs.vulkan.org/spec/latest/chapters/clears.html)
- [OpenGL 4.6 Core specification](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)
- [ADR 0005: WorldCompiler repository split gate](0005-world-compiler-repository-split-gate.md)
- [ADR 0009: portable terrain stage and package](0009-portable-terrain-stage-and-package.md)
- [ADR 0011: Vulkan transfer clear and renderer selection](0011-vulkan-transfer-clear-and-renderer-selection.md)
