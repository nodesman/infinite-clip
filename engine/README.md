Bullet Engine (C++17)

Overview
- Pure transform engine implementing the outliner rules in `docs/BULLET_CANVAS_SPEC.md`.
- Library target: `bullet_engine`
- Test executable: `engine_tests` (assert-based; no external framework).

Build
- `mkdir -p build && cd build`
- `cmake ..`
- `cmake --build .`
- `./engine_tests`

Notes
- IDs auto-generate as `n1`, `n2`, ... via `State::idCounter`.
- `initial_state()` creates a single empty root node focused.
- `apply_command(state, command)` returns a new `State` by value.

