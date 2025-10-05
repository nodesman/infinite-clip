Bullet Canvas Outliner

- See full spec in `docs/BULLET_CANVAS_SPEC.md`.
- This repo will house a reusable outliner component and engine to be consumed by multiple apps.

Structure
- `engine/` — C++17 core engine (pure transforms), with simple assert-based tests.
- `docs/` — cross-framework spec and design.
- `web/` — placeholder for WebAssembly bindings and Svelte wrapper.

Build (engine)
- Prereq: CMake + a C++17 compiler.
- Commands:
  - `mkdir -p engine/build && cd engine/build`
  - `cmake .. && cmake --build .`
  - `./engine_tests`
