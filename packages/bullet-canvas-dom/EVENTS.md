Bullet Canvas DOM — Event Spec
================================

This document defines the events emitted by the BulletCanvas DOM library, how to listen and optionally intercept them, and what data they carry. All events are dispatched as CustomEvent from the container element you pass to new BulletCanvas(...).

Design Goals
- Framework‑agnostic, DOM‑native events.
- Stable, typed payloads for common integration tasks (analytics, persistence, custom commands).
- Interception points (cancelable) for advanced control without forking the library.
- Coherent ordering: before:* (cancelable) → mutation → after:* (non‑cancelable) → change.

Event Targets
- Dispatched on the container element passed to the constructor.
- Events do not bubble outside of container (bubbles: false); attach listeners directly on container.

Event Names and Payloads

Notation: CustomEvent<Detail> — detail shape shown below (TypeScript style).

High‑level
- change: CustomEvent<{ state: State; lastOp?: OpSummary }>
  - Emitted after any successful mutation (including text input).
  - state: deep clone of current state (use for persistence).
  - lastOp: summary of the operation that triggered the change.

- focus: CustomEvent<{ id: string; caret: number }>
  - Emitted when the focused node or caret changes due to user action or API.

- drilldown: CustomEvent<{ id: string | null; oldId: string | null }>
  - Emitted when scope root changes (glyph click or API setScopeRoot).

Operation lifecycle
For each command, a pair of events MAY be emitted:
- before:<op>: cancelable; call event.preventDefault() to abort the operation.
- after:<op>: non‑cancelable.

All before:* fire before state changes; all after:* fire after state changes and before the top‑level change.

Common detail fields:
- trigger: 'keyboard' | 'api' | 'pointer' | 'paste'
- activeId: string (focused id at trigger time)
- caret?: number
- meta?: Record<string, unknown> (reserved per op)

Ops:
- before:split, after:split — { id: string; caret: number; newId?: string }
- before:insertSibling, after:insertSibling — { id: string; newId?: string; index?: number }
- before:indent, after:indent — { id: string; prevSiblingId?: string; newParentId?: string }
- before:outdent, after:outdent — { id: string; parentId?: string; newParentId?: string }
- before:moveUp/down, after:moveUp/down — { id: string; fromIndex: number; toIndex: number; parentId: string | null }
- before:deleteEmpty, after:deleteEmpty — { id: string; parentId: string | null }
- before:mergeNext, after:mergeNext — { id: string; mergedId?: string }
- before:setScopeRoot, after:setScopeRoot — { oldId: string | null; id: string | null }
- before:textInput, after:textInput — { id: string; before: string; after: string }
- before:paste, after:paste — { id: string; lines: string[]; createdIds?: string[] }

Transaction event (optional aggregation)
- transaction: CustomEvent<{ steps: OpSummary[]; origin: 'keyboard' | 'api' | 'pointer' | 'paste' }>
  - Emitted for multi‑step mutations that logically belong together (e.g., multi‑line paste). change still fires once at the end.

OpSummary Type (TypeScript)
export type OpSummary =
  | { type: 'split'; id: string; caret: number; newId: string }
  | { type: 'insertSibling'; id: string; newId: string; index: number }
  | { type: 'indent'; id: string; newParentId: string }
  | { type: 'outdent'; id: string; newParentId: string | null }
  | { type: 'moveUp' | 'moveDown'; id: string; fromIndex: number; toIndex: number; parentId: string | null }
  | { type: 'deleteEmpty'; id: string; parentId: string | null }
  | { type: 'mergeNext'; id: string; mergedId: string }
  | { type: 'setScopeRoot'; oldId: string | null; id: string | null }
  | { type: 'textInput'; id: string; before: string; after: string }
  | { type: 'paste'; id: string; lines: string[]; createdIds: string[] };

Ordering Guarantees
- Single action (e.g., Enter split):
  1) before:<op> (cancelable)
  2) state mutation
  3) after:<op> (non‑cancelable)
  4) change with { state, lastOp }
- Composite action (e.g., multi‑line paste): many before/after pairs, then one transaction, then one change.

Cancelation & Overrides
- before:* events are cancelable. If you call event.preventDefault(), the library will skip the default mutation.
- You may then call canvas.setState(...), or perform your own mutations followed by your own change if desired.
- Not all operations are cancelable in practice (e.g., browser‑driven focus); only operations controlled by the library emit before:*.

Practical Hooks
- Persistence: listen to change and save detail.state (debounce recommended).
- Analytics / Telemetry: listen to after:* and transaction to track usage.
- Custom key bindings: listen to before:split / before:indent etc., preventDefault(), then perform custom mutations.
- Drill‑down integration: listen to drilldown to sync app location/breadcrumbs.
- Read‑only mode: intercept before:* mutations and preventDefault() when locked.
- External selection syncing: listen to focus; call focusNode(id, caret) to restore.
- Paste normalization: intercept before:paste, sanitize lines, perform your own inserts, preventDefault().

Performance Notes
- change includes a cloned state; if your state is large, prefer after:* op events for lighter payloads and keep your own state store.
- Text input can be noisy; you may debounce after:textInput if desired.
- Guides are drawn post‑render via requestAnimationFrame; no events are emitted for them.

Backwards Compatibility
- Event names and payloads are semver‑governed. Adding new after:* types is a minor; changing/removing fields is a major.

Examples
// Save on change
container.addEventListener('change', e => saveToDb(e.detail.state));

// Prevent indent under certain ids
container.addEventListener('before:indent', e => {
  if (shouldBlockIndent(e.detail.id)) e.preventDefault();
});

// Track drilldown
container.addEventListener('drilldown', e => {
  console.log('scope changed', e.detail.oldId, '→', e.detail.id);
});

