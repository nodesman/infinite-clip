Browser Demo

- Run without installs: open `web/demo/index.html` in a browser.
- Uses a small JS engine mirroring the C++ semantics to let you try the interactions now.
- Next step: replace the JS engine by compiling the C++ core to WebAssembly via Emscripten and exposing `applyCommand` to JS.

Planned wasm bridge
- Build with Emscripten (example using Embind):
  - `emcmake cmake -S engine -B engine/build-wasm -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build engine/build-wasm --target bullet_engine_wasm`
  - Output will be something like `engine/build-wasm/bullet_engine_wasm.js` + `.wasm`.
- JS usage sketch:
  - `import createModule from './bullet_engine_wasm.js';`
  - `const Module = await createModule();`
  - `const engine = new Module.Engine();`
  - `engine.applyCommand(CommandType.Indent, id, -1, ''); // see below`
  - Exposed methods: `applyCommand(type, id, caret, scopeRoot)`, `focusedId()`, `caret()`, `getText(id)`, `setText(id,text)`, `prevVisible(id)`, `nextVisible(id)`, `rootOrder()`, `children(id)`.
  - CommandType values (ints) map to C++ enum: 0 InsertEmptySiblingAfter, 1 SplitAtCaret, 2 Indent, 3 Outdent, 4 MoveUp, 5 MoveDown, 6 DeleteEmptyAtId, 7 MergeNextSiblingIntoCurrent, 8 SetFocus, 9 SetScopeRoot.

Note: For parity, the C++ engine remains the source of truth with comprehensive tests.
