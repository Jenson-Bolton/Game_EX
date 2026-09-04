# Coding standards

- Use portable C++20 and standard-library facilities unless an accepted decision adds a dependency.
- Prefer explicit ownership with values and `std::unique_ptr`; shared ownership requires justification.
- Do not introduce service locators or subsystem singletons.
- Keep native, SDL, OpenGL, and Vulkan types behind their backend implementation boundaries.
- Treat warnings as defects. Current targets compile with `/W4 /permissive-` on MSVC and a strict warning set elsewhere.
- Use `PascalCase` for types, `snake_case` for functions/variables, and lowercase namespaces.
- Prefer scoped enums, `[[nodiscard]]` for ignored-result hazards, and narrow interfaces.
- Validate inputs at ownership or subsystem boundaries and report actionable errors.
- Preserve deterministic ordering and seeded randomness where simulation results are concerned.
- Avoid placeholder modules and abstractions with no current caller.

Every C++ file needs an `@file` description. Document public types, functions, parameters, returns, thrown errors, ownership/lifetime, units, invariants, and non-obvious internal algorithms using Doxygen syntax. Comments should explain contracts and reasoning rather than restate syntax.
