# Project vision

Game_EX has two related goals:

1. Build a reusable, modular C++ engine for large worlds, deterministic simulation, strong multithreading, and replaceable platform/rendering backends.
2. Build its first game: a transport, city, and land-use simulation capable in the long term of representing the whole Czech Republic as a playable world.

The engine remains unaware of railways, Czech Republic datasets, population, economics, and planning rules. The game remains unaware of SDL objects, operating-system handles, and OpenGL or Vulkan objects.

An offline world compiler is a first-class programme rather than a runtime convenience. It will eventually reconcile terrain, authoritative geography, buildings, transport networks, statistics, and semantic layers such as TESSERA into streamable world packages. The shipped game reads those packages through a small versioned WorldFormat library.

The project values explicit dependencies, deterministic behaviour, testable ordinary objects, headless-capable simulation, and measured data-oriented design. ECS may be useful for visible and interactive objects, but national transport graphs, terrain tiles, land parcels, timetables, and semantic grids require domain-specific structures rather than an “everything is an entity” rule.
