# Vendored generated GLAD loader

This directory is the narrow exception to the project rule that downloaded and
generated dependencies stay under `build/`. Game_EX vendors only the generated
loader files needed by supported builds, so contributors do not need Python or
Jinja to compile the engine. The GLAD generator itself is not vendored.

## Provenance

- upstream: `https://github.com/Dav1dde/glad`
- upstream tag: `v2.0.8`
- official tag archive: `https://github.com/Dav1dde/glad/archive/refs/tags/v2.0.8.tar.gz`
- archive SHA-256: `44F06F9195427C7017F5028D0894F57EB216B0A8F7C4EDA7CE883732AEB2D0FC`
- generator: GLAD 2.0.8, C generator
- generator-only template runtime used for this reproduction: Jinja2 3.1.6 and MarkupSafe 3.0.2
- arguments: `--api gl:core=4.6 --extensions '' --out-path <output> --reproducible c`
- options implied by the command: no aliases, debug wrappers, internal loader,
  multi-context mode, or on-demand loading

The reproducible flag makes GLAD use the Khronos XML specifications included in
the tag archive. The logged remote URLs are intercepted by GLAD and are not
downloaded. OpenGL procedures are resolved at runtime through SDL. GLAD's text
writer follows the host newline convention, so the generated text is normalized
to UTF-8 without a byte-order mark and LF line endings before hashing. Git
attributes pin that committed representation; the hashes below are the
post-normalization bytes a fresh checkout receives. The generated C and header
paths are exempt from Git's project whitespace checker so generator-authored
spacing is preserved byte-for-byte; only newline encoding is normalized.

## Checked-in files

| File | SHA-256 |
| --- | --- |
| `include/glad/gl.h` | `0E1AC72E82AEC8AB8D1FE176E46A471643C9673B03BA42C4DF6DEA89D2F0E7F5` |
| `include/KHR/khrplatform.h` | `7B1E01AAA7AD8F6FC34B5C7BDF79EBF5189BB09E2C4D2E79FC5D350623D11E83` |
| `src/gl.c` | `07CF9E52E7C018A1E5A8FDE75BFE63F0527B3963B9F5F6219AE04924FBE934C1` |
| `LICENSE` | `CBB325CD4AC5BD06717FFA34A322B984F9D285E4C67C8CA1B7380D6EF08C2A77` |

`Engine/CMakeLists.txt` verifies the three generated-file hashes at configure
time. The copied, LF-normalized upstream `LICENSE` records the GLAD generator
source's MIT terms and the applicable Khronos notices. The generated `gl.h` and
`gl.c` instead carry the SPDX expression `(WTFPL OR CC0-1.0) AND Apache-2.0` in
their own headers. `khrplatform.h` retains its separate Khronos permissive
copyright and permission notice. Those generated-file notices, not an inferred
project licence, govern the corresponding vendored outputs.
