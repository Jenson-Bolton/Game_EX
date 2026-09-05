# Testing

CTest is the common runner. Tests use these labels:

- `unit`: deterministic dependency-light logic;
- `gui`: a real desktop window/display is required;
- `smoke`: a production-shaped lifecycle must complete;
- `opengl` or `vulkan`: backend-specific evidence;
- `validation`: Khronos Vulkan validation is required, not optional;
- `death`: a subprocess must fail fast for a forbidden ownership action.

Run everything from a configured workspace:

```powershell
ctest --test-dir build/vs2022 -C Debug --output-on-failure
```

## Rendering evidence

Neutral Render tests validate the exact linear diagnostic colour, owning
background/raster frame, inclusive finite `[0, 1]` bounds, `64 x 64`/4,096-cell
limits, exact colour storage, finite positive aspect, backend text, and
categorized error retention. Pure layout tests cover aspect fitting,
letterboxing, odd pixel partitions, full content coverage, lower-row ordering,
and drawables smaller than the raster. Fake Application tests record exact
ordering for window capability, renderer start, hidden presentation attempt,
show, visible presentation, hide, renderer shutdown/destruction, window
destruction, and platform destruction. They prove:

- renderer-start and initial-frame failure remain pre-visibility;
- a hidden zero-extent deferral may be followed by show and a real visible present;
- a timed run with permanent deferral fails `presentation_failed` rather than
  passing because its timer expired;
- visibility evidence remains true for success, deferred-show, runtime failure,
  and timed no-present paths;
- a native-frame failure is terminal until renderer shutdown;
- one immutable owning raster reaches both hidden and visible attempts with no
  borrowed editor lifetime;
- partial start and runtime failures preserve reverse cleanup.

Pure Vulkan policy tests shuffle inputs and lock deterministic device and queue
selection. They cover combined/separate queues, unsuitable devices, sRGB-only
format preference, single undefined-format handling, empty/non-sRGB rejection,
fixed and clamped extents, bounded/unlimited/inconsistent image counts, and
composite-alpha preference/rejection.

The real OpenGL smoke verifies an actual 4.6 Core sRGB context, a 3 x 2 raster
through scissored clears, restored scissor state, and a presentation after
visibility. The normal Vulkan smoke requires a real Vulkan 1.3 device with
synchronization2 and dynamic rendering, an sRGB FIFO transfer/colour-attachment
swapchain, a 2 x 2 attachment-clear raster, and at least 32 visible
presentations. The validation smoke repeats that batch with required Khronos
validation and fails when `debug_error_count` is non-zero. Per-image semaphore
and image-view use are exercised across repeated acquisition rather than
inferred from timer survival.

Validation tests make loader discovery deterministic: `VK_LAYER_PATH` points at
the selected SDK `Bin`; `VK_IMPLICIT_LAYER_PATH` points at an existing empty
build directory; additive/inherited layer controls are unset; and
`VK_LOADER_LAYERS_DISABLE=~implicit~`. This prevents third-party overlays or
stale registry JSON from contaminating engine evidence. Normal Vulkan-creating
smokes use the same empty implicit-layer boundary without forcing validation.

Game and world-editor shell smokes explicitly run both OpenGL and Vulkan. A
CTest setup fixture compiles the checked-in synthetic staging text to a
test-owned package; separate editor data smokes load it under each API and match
the expected 2 x 2/three-valid/one-invalid summary. A pre-window integration
check supplies missing and corrupt packages, requires a package diagnostic, and
rejects any `Trying renderer:` marker. A separate auto smoke proves the
production default path but is not evidence that both backends work. Every
timed application smoke succeeds only after at least one renderer-confirmed
presentation.

Pure world-editor tests cover editor option extraction, exact
blue-green-yellow mapping, row-zero-lower orientation, flat/all-invalid fields,
partial-validity blending, sample-footprint aspect, summary provenance, and the
deterministic 251 x 201 to 64 x 64 overview bound.

## Selection evidence

Pure parser tests cover omission/default auto, explicit auto/OpenGL/Vulkan,
option order, and rejection of empty, unknown, repeated, conflicting, malformed,
signed, or excessive arguments. Injected attempts prove:

- explicit choices make one attempt and never fall back;
- auto success makes one Vulkan attempt;
- auto fallback order is exactly Vulkan then OpenGL;
- only unavailable/initialization failures from Vulkan before visibility qualify;
- post-visibility unavailable/initialization, presentation, shutdown, wrong
  backend evidence, and empty callables cannot authorize fallback;
- the preferred failure reaches the fallback observer;
- a failed OpenGL fallback propagates its own cause after exactly two attempts.

## Other retained suites

Startup and JobSystem suites retain deterministic graph validation, admission,
affinity, concurrency, exception ordering, rollback, stopped-state, and
wrong-thread death evidence. WorldCompiler retains strict stage/package parsing,
full-byte golden comparison, runtime-only reading, CRC/corruption/version/bounds,
UTF-8 staged paths, provenance, transactional publication, and race tests. The
committed terrain fixture remains synthetic; in this version it proves both
codec behaviour and editor wiring, never real geographic accuracy.

Set `GAMEEX_ENABLE_GUI_SMOKE_TESTS=OFF` only in a genuinely headless build. A
green non-GUI suite does not prove either graphics backend. Future visual tests
must define capture colour space and tolerances; `v0.1.7` proves semantic frame
inputs, API command execution, diagnostics, and presentation but intentionally
makes no captured-pixel equality claim. The bounded per-cell viewer is not performance evidence
for full-resolution, multi-layer, or interactive rendering.
