# Contributing to Game_EX

Game_EX is at a specification-first foundation stage. Discuss changes that introduce a new subsystem, third-party dependency, persistent file format, or cross-project dependency before implementation, and record the accepted decision in `docs/decisions`.

All C++ uses C++20, builds warning-clean at the configured warning level, and follows the ownership and dependency rules in the architecture documentation. Public APIs and non-obvious internal behaviour must be documented with Doxygen comments. A change is ready when its relevant tests and the `docs` target pass.

See [coding standards](docs/development/coding-standards.md), [testing](docs/development/testing.md), and [adding a module](docs/development/adding-a-module.md).
