# Bullet Canvas Outliner — Cross‑Framework Spec

## Purpose
- Reusable outliner ("bullet canvas") with unlimited depth and strict, predictable keyboard semantics.
- DOM/widget-first: each bullet is its own element with hover UI and an action stub.
- Architecture supports drill‑down (any node can become viewport root) without changing the engine.

## Scope & Platforms
- Primary clients: Svelte (web) and C++/Qt (desktop).
- Shared core: a pure “engine” of tree transforms + selection model. Views map events to engine commands.
- No `<ol>/<li>` semantics; strict custom DOM/widgets to enable styling/hover/control over interactions.

## UI Model
- Canvas: dark grey background.
- Row per bullet: `bulletGlyph` (circular element), `textEditor` (sanitized single-line content), `actionMenu` stub `(…)` (light grey circle) shown on hover/focus.
- Hover: bullet glyph shows thick circular grey border.
- Click anywhere on the text focuses the bullet.
- Depth indent via left padding; no list semantics.

## Data Model
- Node: `{ id: string, parentId: string|null, text: string, children: string[], collapsed?: boolean }`.
- Tree storage:
  - `nodes: Map<string, Node>` — authoritative store of nodes by id.
  - `rootOrder: string[]` — ordered root-level nodes.
- View state:
  - `focusedId: string` — the focused node id.
  - `caret: number` — caret offset within the focused node’s text.
  - `scopeRootId?: string|null` — node id whose subtree is currently the viewport root; null/undefined means full tree.
- Invariants:
  - Sibling order is defined by their container list (`children` or `rootOrder`).
  - Indent only under immediate previous sibling (no level skipping); if none, Tab is no-op.
  - Children move with their parent for all structural operations unless explicitly stated otherwise.
  - Always at least one root node exists; cannot delete the last remaining root node.

## Keyboard Semantics

### Enter
- Mid-text (caret within text): split current node at caret into two sibling nodes.
  - The second node receives all of the original node’s children.
  - The first node’s children become empty `[]`.
  - Focus the second node, caret at start (0).
- End-of-text (non-empty): create an empty sibling immediately after at same level. Focus the new node, caret 0.
- Empty and indented: outdent one level. Repeated Enter keeps outdenting until root; at root, Enter creates an empty root sibling after.
- Empty and root: creates an empty root sibling after.
- Only root level allows creating new empty bullets directly via Enter.

### Tab / Shift+Tab
- Tab (indent): indent current node under its immediate previous sibling; becomes that sibling’s last child. If no previous sibling → no-op. Never skip levels.
- Shift+Tab (outdent): current node becomes the next sibling of its former parent. Entire subtree moves.

### Reorder (Cmd/Ctrl + Shift + Arrows)
- Up: move up among siblings. If already first, hoist to become previous sibling of its parent (repeatable up to root). Subtree moves with the node.
- Down: move down among siblings. If already last, sink to become next sibling of its parent. Subtree moves with the node.
- Modifiers: macOS uses Cmd; Windows/Linux use Ctrl.

### Backspace & Delete
- Backspace at start with text: do nothing (no merge with previous).
- Backspace on an empty node with no children: delete that node. Refocus previous visible node (else next). If this would delete the last remaining root node, instead clear its text (keep focused, empty).
- Delete key at end of text:
  - If the next node is an immediate sibling AND current node has no children: merge the next sibling’s text to the end of current; move next sibling’s children to become current’s children (append in order); remove the next sibling. Caret ends at original end + merged text length.
  - If the next node is a child (or there is no next sibling): no-op (beyond normal character deletion within text).
- Backspace and Delete otherwise behave as normal character deletion within the text.

### Arrow Navigation
- Up: move focus to the beginning of the previous visible bullet (sibling or parent, per visual order), regardless of current caret column.
- Down: move focus to the beginning of the next visible bullet.

### Paste
- Pasting multi-line `text/plain` inserts that many sibling bullets at the same level, starting after the current node (or splitting the current text if desired by view, but default is insertion after current node). Focus the last inserted bullet; caret at end of its text.
- Sanitize strictly: accept only `text/plain`; strip HTML; reject images/files.

## Drill‑Down (Scope)
- `scopeRootId` sets the viewport root; only nodes in its subtree are rendered.
- All engine operations use global ids/parentage; scope is a view filter only.
- Provide breadcrumb path from `scopeRootId` to root: display node texts; fallback to id snippet if text empty.
- Future: clicking bullet glyph (or an affordance) sets `scopeRootId = clickedId` (drill‑down). Command to clear scope returns to full tree.

## Engine API (Pure Transforms)
- All functions are pure: `(state) => newState`, where state is `{ nodes, rootOrder, focusedId, caret, scopeRootId? }`.
- Suggested operations:
  - `insertEmptySiblingAfter(id)`
  - `splitAtCaret(id, caret)`
  - `indent(id)`
  - `outdent(id)`
  - `moveUp(id)`
  - `moveDown(id)`
  - `deleteEmptyAtId(id)`
  - `mergeNextSiblingIntoCurrent(id)` (preconditions enforced: current has no children, next exists and is sibling)
  - `setFocus(id, caret)`
  - `setScopeRoot(id|null)`

### Algorithms (High Level)
- `splitAtCaret(id, caret)`:
  1. Create `newId`.
  2. `new.text = id.text.slice(caret)`; `id.text = id.text.slice(0, caret)`.
  3. Insert `newId` right after `id` in its sibling list.
  4. Move `id.children` to `new.children`; set `id.children = []`.
  5. Focus `newId`, caret 0.
- `indent(id)`:
  1. Find `prevSibling`. If none → no-op.
  2. Remove `id` from current siblings.
  3. Append `id` to `prevSibling.children`.
  4. Focus preserved.
- `outdent(id)`:
  1. If `parentId` is null → no-op.
  2. Remove `id` from parent’s children.
  3. Insert `id` as the next sibling after its parent in the parent’s sibling list (parent’s parent or rootOrder).
  4. Focus preserved.
- `moveUp(id)`:
  1. If has previous sibling: swap.
  2. Else if parent exists: hoist — insert before parent in parent’s siblings (or rootOrder). Keep subtree.
  3. Else (root and first): no-op.
- `moveDown(id)`:
  1. If has next sibling: swap.
  2. Else if parent exists: sink — insert after parent in parent’s siblings. Keep subtree.
  3. Else (root and last): no-op.
- `mergeNextSiblingIntoCurrent(id)`:
  1. Check next sibling `nextId` exists and current has no children; else no-op.
  2. `id.text += next.text`.
  3. Move `next.children` to end of `id.children`.
  4. Remove `nextId` from siblings and `nodes`.
  5. Set caret to end of merged `id.text`.
- `deleteEmptyAtId(id)`:
  1. If node has children → no-op.
  2. If deleting would leave zero root nodes → instead set `id.text = ''` and keep focus.
  3. Else remove node and compute new focus: prefer previous visible node, else next.

## Focus & Caret Rules
- After split: focus second node, caret 0.
- After creating empty sibling: focus new node, caret 0.
- After outdent via Enter (empty): focus remains on the same node, caret 0.
- After move up/down: focus stays on moved node, caret unchanged when possible (or set to start if uncertain).
- After delete-empty: focus previous visible node (else next), caret at end.
- Up/Down navigation always places caret at the start of the target node’s text.

## Sanitization & Input Policy
- Web (Svelte):
  - Use `contenteditable` only for the text region; intercept input to ensure plain text only.
  - `beforeinput`: prevent formatting/editing commands such as `formatBold`, `insertImage`, `insertParagraph`, `insertFromDrop`, etc.
  - `paste`: prevent default; read `event.clipboardData.getData('text/plain')`; split and insert lines.
  - Prevent HTML injection by never trusting `innerHTML`; update via textContent/controlled bindings.
  - Prevent images/attachments (ignore non-plain types).
  - Treat Enter as command (prevent newline); the editor remains visually single-line.
- Qt:
  - Use `QLineEdit` or `QPlainTextEdit` constrained to single line; custom key filter.
  - `QClipboard::text()` for paste; split and insert; ignore HTML/MIME rich data.

## Cross‑Platform Keys
- macOS: Cmd+Shift+Up/Down for reordering.
- Windows/Linux: Ctrl+Shift+Up/Down.
- Tab/Shift+Tab, Enter, Backspace, Delete consistent across platforms.

## Styling Tokens (suggested)
- `--canvas-bg: #1f1f1f`
- `--text-fg: #e6e6e6`
- `--bullet-hover: #8a8a8a` (thick circular border)
- `--menu-bg: #3a3a3a` (lighter than canvas)
- `--menu-fg: #d7d7d7`
- Indent per depth: `padding-left = depth * 16px`

## Testing Matrix
- Enter:
  - Split with/without children; second gets children; focus/caret.
  - Empty indented outdents repeatedly to root; at root creates empty root sibling.
  - Non-empty end-of-text creates empty sibling.
- Tab/Shift+Tab:
  - Tab with previous sibling; Tab with none (no-op). No level skipping.
  - Shift+Tab at various depths; root no-op.
- Reorder:
  - Move within siblings; hoist/sink at bounds (preserve subtree).
- Backspace/Delete:
  - Backspace at start with text: no-op.
  - Backspace empty with no children: delete; guard last root → clear only.
  - Delete at end merges only when next is sibling and current has no children; children transfer.
- Navigation:
  - Up/Down anywhere in text; focus start of previous/next visible node.
- Paste:
  - Multi-line plain text → multiple siblings; focus last inserted.
  - HTML or images ignored; only `text/plain` processed.

## Reference View Notes

### Svelte
- Components:
  - `BulletList.svelte`: manages state, scope root, global key bindings, renders visible subtree.
  - `BulletItem.svelte`: one node row; owns `contenteditable` span, glyph, menu. Emits events to parent for engine commands.
  - `engine.ts`: pure transform functions and state helpers.
- Event handling:
  - `keydown` on editor: intercept Enter/Tab/Shift+Tab/Cmd|Ctrl+Shift+Arrows/Backspace/Delete.
  - `beforeinput`/`paste`: sanitize to plain text only; prevent non-text inputs.
  - Maintain caret via Selection APIs; compute offsets on input, set after transforms.

### C++/Qt
- Widgets:
  - `BulletListView` (scroll area) renders rows for visible subtree.
  - `BulletRowWidget` holds glyph, `QLineEdit`/single-line `QPlainTextEdit`, and menu button.
  - Engine as a separate class with pure methods; integrate via signals/slots.
- Event handling:
  - Reimplement `keyPressEvent` to intercept keys and call engine transforms.
  - Use `QClipboard::text()` for paste; ignore rich data.

## Undo/Redo
- Maintain a command stack with reversible ops or state snapshots.
- `Ctrl/Cmd+Z` undo, `Shift+Ctrl/Cmd+Z` redo.
- Coalesce rapid text inputs; treat structural edits (split/indent/outdent/move/merge/delete) as discrete steps.

## Future Extensions
- Collapse/expand nodes (toggle `collapsed`).
- Drill‑down UI (glyph long‑press or menu action).
- Action menu: move/copy, duplicate, mark complete, color tags.
- Persistence adapters (local storage, file, backend) — outside current scope.

## Decisions (Confirmed)
- Keep one root minimum; if deletion would remove last root, clear text instead.
- Breadcrumb uses node texts (fallback to id snippet).
- After split: focus second, caret at start.
- After multi-line paste: focus last inserted.
- Tab no-op when no previous sibling (even non-root).
- Up/Down can be pressed anywhere; focus moves to start of previous/next visible bullet.
- Web uses sanitized `contenteditable` (plain text only; block images/HTML). Desktop uses plain text widgets.

## Glossary
- Hoist: move node to become a sibling of its parent, before the parent.
- Sink: move node to become a sibling of its parent, after the parent.
- Visible order: preorder traversal of the subtree rooted at `scopeRootId` (or all roots when not scoped), respecting sibling order and collapsed flags.

