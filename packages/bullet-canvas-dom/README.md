Bullet Canvas DOM (framework‑agnostic)

A plain DOM outliner component with unlimited depth, keyboard interactions, drill‑down (scoped root as heading), breadcrumbs, and measured vertical guides aligned to bullet centers. No frameworks, no web components — just DOM + CSS variables. Works in any app.

Quick start
- Include CSS (tokens + base styles):
  - JS/ESM: `import 'bullet-canvas-dom/src/styles.css'`
  - Or copy the variables into your global stylesheet.
- Instantiate:
  - `import { BulletCanvas } from 'bullet-canvas-dom/src/index.js'`
  - `const canvas = new BulletCanvas(document.getElementById('mount'), { autofocus: true, showBreadcrumbs: true });`

API
- `new BulletCanvas(container, options?)`
  - `options.value?: State` initial state (defaults to a single root node)
  - `options.autofocus?: boolean`
  - `options.showBreadcrumbs?: boolean`
  - `options.tokens?: Partial<CssTokens>` apply CSS variables on the host element
- Methods
  - `getState(): State`
  - `setState(state: State): void`
  - `applyCommand(cmd: Command): void`
  - `setScopeRoot(id: string | null): void`
  - `focusNode(id: string, caret?: number): void`
  - `destroy(): void`
- Events (CustomEvent dispatched on `container`)
  - `change` → `{ state }`
  - `focus` → `{ id, caret }`
  - `drilldown` → `{ id }`

Styling tokens (CSS variables on the container)
- `--indent-step` (default 40px)
- `--root-left-pad` (default 44px)
- `--bullet-size` (8px)
- `--bullet-ring` (6px)
- `--bullet-fill` (#9a9a9a)
- `--bullet-border-hover` (#5e5e5e)
- `--menu-size` (22px)
- `--menu-gap` (8px)
- `--glyph-gap` (12px)
- `--guide-color` (#8a8a8a)
- `--guide-width` (1px)
- `--guide-offset` (0px)

Notes
- Scoped (drilled‑down) root is shown as a heading (no visible bullet) and cannot be deleted. Hitting Enter in the heading creates a new child and focuses it.
- Vertical guides are measured to real bullet centers after render; they stay aligned even if you change sizes or fonts.

