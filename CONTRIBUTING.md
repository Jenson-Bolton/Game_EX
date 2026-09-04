# Contributing to Game_EX

Game_EX is developed through specification-gated, evidence-backed versions. Follow the [working agreement](docs/development/README.md) for scope approval, versioning, release reports, data provenance, and the definition of done.

Discuss changes that introduce a new subsystem, third-party dependency, persistent file format, renderer or platform boundary, coordinate convention, data source, or cross-project dependency before implementation. Record an accepted architectural choice in `docs/decisions`; do not infer a decision from provisional code.

All C++ uses C++20, builds warning-clean at the configured warning level, and follows the ownership and dependency rules in the architecture documentation. Every C++ file and public contract must have complete Doxygen documentation, including units, ownership, invariants, parameters, returns, and errors where relevant. Non-obvious private behaviour must also be documented. Human-authored Markdown must link to generated API documentation instead of duplicating it.

A version is ready only when its implementation, tests, Doxygen, human documentation, changelog entry, provenance records, and technical report agree. Publish that coherent state as one release commit and annotated tag. See [coding standards](docs/development/coding-standards.md), [testing](docs/development/testing.md), [adding a module](docs/development/adding-a-module.md), and the [report template](docs/history/report-template.md).
