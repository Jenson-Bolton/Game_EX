# Adding a module

Before adding a module:

1. State its single responsibility and current caller.
2. Identify which project owns it and draw its dependency direction.
3. Record a decision if it adds a third-party dependency, file format, cross-project edge, thread, or persistent state.
4. Define public contracts independently of backend-specific types.
5. Add implementation and focused tests.
6. Add Doxygen group membership and update the corresponding section document.
7. Build with warnings enabled, run tests, and run the strict `docs` target.

Do not create empty “future” directories or a catch-all `Manager`. A planned programme becomes source structure when a concrete vertical slice needs it.
