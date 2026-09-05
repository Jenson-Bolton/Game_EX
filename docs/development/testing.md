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

Neutral Render tests validate the exact linear diagnostic colour, inclusive
finite `[0, 1]` bounds, backend text, and categorized error retention. Fake
Application tests record exact ordering for window capability, renderer start,
hidden presentation attempt, show, visible presentation, hide, renderer
shutdown/destruction, window destruction, and platform destruction. They prove:

- renderer-start and initial-frame failure remain pre-visibility;
- a hidden zero-extent deferral may be followed by show and a real visible present;
- a timed run with permanent deferral fails `presentation_failed` rather than
  passing because its timer expired;
- visibility evidence remains true for success, deferred-show, runtime failure,
  and timed no-present paths;
- a native-frame failure is terminal until renderer shutdown;
- partial start and runtime failures preserve reverse cleanup.

Pure Vulkan policy tests shuffle inputs and lock deterministic device and queue
selection. They cover combined/separate queues, unsuitable devices, sRGB-only
format preference, single undefined-format handling, empty/non-sRGB rejection,
fixed and clamped extents, bounded/unlimited/inconsistent image counts, and
composite-alpha preference/rejection.

The real OpenGL smoke verifies an actual 4.6 Core sRGB context and a presentation
after visibility. The normal Vulkan smoke requires a real Vulkan 1.3 device,
sRGB FIFO transfer-destination swapchain, and at least 32 visible presentations.
The validation smoke repeats that batch with required Khronos validation and
fails when `debug_error_count` is non-zero. Per-image semaphore reuse is thus
exercised across repeated acquisition rather than inferred from timer survival.

Validation tests make loader discovery deterministic: `VK_LAYER_PATH` points at
the selected SDK `Bin`; `VK_IMPLICIT_LAYER_PATH` points at an existing empty
build directory; additive/inherited layer controls are unset; and
`VK_LOADER_LAYERS_DISABLE=~implicit~`. This prevents third-party overlays or
stale registry JSON from contaminating engine evidence. Normal Vulkan-creating
smokes use the same empty implicit-layer boundary without forcing validation.

Game and world-editor smokes explicitly run both OpenGL and Vulkan. A separate
auto smoke proves the production default path but is not evidence that both
backends work. Every timed application smoke succeeds only after at least one
renderer-confirmed presentation.

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
committed terrain fixture remains synthetic and proves codec behaviour only.

Set `GAMEEX_ENABLE_GUI_SMOKE_TESTS=OFF` only in a genuinely headless build. A
green non-GUI suite does not prove either graphics backend. Future visual tests
must define capture colour space and tolerances; v0.1.6 intentionally makes no
pixel-equality claim.
