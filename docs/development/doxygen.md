# Doxygen policy

Human project documentation lives under `docs`; generated API reference lives only under the active CMake build tree.

The root `Doxyfile.in` scans all current C++ public headers, implementation files, tests, tools, and `docs/doxygen`. It extracts private/static members and anonymous namespaces so internal ownership, algorithms, and invariants are reviewable. Undocumented symbols, parameter mismatches, broken references, and documentation errors fail the `docs` target.

Use:

- `@file` and `@brief` for each C++ file;
- `@param`, `@return`, and `@throws` where applicable;
- `@ingroup` for module ownership;
- comments on data members when units, ownership, or invariants matter;
- `@copydoc` for a backend override whose public contract is unchanged.

Run:

```powershell
cmake --build --preset docs
```

Open `build/vs2022/docs/doxygen/html/index.html`. Generated HTML is ignored and must not be committed.

Graphviz is not a prerequisite at this milestone, so DOT diagrams are disabled. The human architecture documents contain the authoritative dependency views.
