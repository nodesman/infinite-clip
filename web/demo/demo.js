// Minimal JS engine mirroring the C++ rules to run in browser without builds.
// State and Node shapes are analogous to the C++ engine.

const newId = (() => { let c = 0; return () => `j${++c}`; })();

function initialState() {
  const id = newId();
  return {
    nodes: { [id]: { id, parentId: '', text: '', children: [] } },
    rootOrder: [id],
    focusedId: id,
    caret: 0,
    scopeRootId: null,
  };
}

function visibleOrderIds(state) {
  const out = [];
  const dfs = (id) => { out.push(id); state.nodes[id].children.forEach(dfs); };
  if (state.scopeRootId && state.nodes[state.scopeRootId]) { dfs(state.scopeRootId); return out; }
  state.rootOrder.forEach(dfs); return out;
}

function siblingsRef(state, id) {
  const p = state.nodes[id].parentId;
  return p ? state.nodes[p].children : state.rootOrder;
}
function indexInSiblings(state, id) { const a = siblingsRef(state, id); return a.indexOf(id); }
function prevSiblingId(state, id) { const a = siblingsRef(state, id); const i = a.indexOf(id); return i>0?a[i-1]:''; }
function nextSiblingId(state, id) { const a = siblingsRef(state, id); const i = a.indexOf(id); return (i>=0 && i+1<a.length)?a[i+1]:''; }
function prevVisibleId(state, id){ const a=visibleOrderIds(state); const i=a.indexOf(id); return i>0?a[i-1]:''; }
function nextVisibleId(state, id){ const a=visibleOrderIds(state); const i=a.indexOf(id); return (i>=0&&i+1<a.length)?a[i+1]:''; }

function setFocus(state, id, caret=0){ state.focusedId=id; state.caret = Math.max(0, Math.min(caret, state.nodes[id].text.length)); }

function insertEmptySiblingAfter(state, id){
  const node = state.nodes[id];
  const nid = newId();
  state.nodes[nid] = { id: nid, parentId: node.parentId, text:'', children: [] };
  const sibs = siblingsRef(state, id); const idx = sibs.indexOf(id); sibs.splice(idx+1,0,nid);
  setFocus(state, nid, 0);
}

function splitAtCaret(state, id, caret){
  const node = state.nodes[id];
  if (caret == null) caret = state.caret;
  caret = Math.max(0, Math.min(caret, node.text.length));
  const nid = newId();
  state.nodes[nid] = { id: nid, parentId: node.parentId, text: node.text.slice(caret), children: node.children.slice() };
  // reparent children to new node
  node.children.forEach(cid => state.nodes[cid].parentId = nid);
  node.children = [];
  node.text = node.text.slice(0, caret);
  const sibs = siblingsRef(state, id); const idx = sibs.indexOf(id); sibs.splice(idx+1,0,nid);
  setFocus(state, nid, 0);
}

function indent(state, id){ const prev = prevSiblingId(state,id); if (!prev) return; const sibs = siblingsRef(state,id); sibs.splice(sibs.indexOf(id),1); state.nodes[id].parentId = prev; state.nodes[prev].children.push(id); }
function outdent(state, id){ const n=state.nodes[id]; if(!n.parentId) return; const p=n.parentId; const gp=state.nodes[p].parentId; const pc=state.nodes[p].children; pc.splice(pc.indexOf(id),1); if(!gp){ const i=state.rootOrder.indexOf(p); state.rootOrder.splice(i+1,0,id); n.parentId=''; }else{ const gpc=state.nodes[gp].children; const i=gpc.indexOf(p); gpc.splice(i+1,0,id); n.parentId=gp; } }
function moveUp(state, id){ const n=state.nodes[id]; const sibs=siblingsRef(state,id); const i=sibs.indexOf(id); if(i>0){ [sibs[i-1],sibs[i]]=[sibs[i],sibs[i-1]]; return; } if(!n.parentId) return; const p=n.parentId, gp=state.nodes[p].parentId; state.nodes[p].children.splice(state.nodes[p].children.indexOf(id),1); if(!gp){ const j=state.rootOrder.indexOf(p); state.rootOrder.splice(j,0,id); n.parentId=''; } else { const gpc=state.nodes[gp].children; const j=gpc.indexOf(p); gpc.splice(j,0,id); n.parentId=gp; } }
function moveDown(state, id){ const n=state.nodes[id]; const sibs=siblingsRef(state,id); const i=sibs.indexOf(id); if(i+1<sibs.length){ [sibs[i],sibs[i+1]]=[sibs[i+1],sibs[i]]; return; } if(!n.parentId) return; const p=n.parentId, gp=state.nodes[p].parentId; state.nodes[p].children.splice(state.nodes[p].children.indexOf(id),1); if(!gp){ const j=state.rootOrder.indexOf(p); state.rootOrder.splice(j+1,0,id); n.parentId=''; } else { const gpc=state.nodes[gp].children; const j=gpc.indexOf(p); gpc.splice(j+1,0,id); n.parentId=gp; } }

function deleteEmptyAtId(state, id){ const n=state.nodes[id]; if(!n || n.text.length>0) return; if(n.children.length) return; const isRoot = !n.parentId; if(isRoot && state.rootOrder.length===1){ n.text=''; setFocus(state, id, 0); return; }
  const prev = prevVisibleId(state,id), next=nextVisibleId(state,id);
  const sibs=siblingsRef(state,id); sibs.splice(sibs.indexOf(id),1); delete state.nodes[id];
  if(state.rootOrder.length===0){ const rid=newId(); state.nodes[rid]={id:rid,parentId:'',text:'',children:[]}; state.rootOrder=[rid]; setFocus(state,rid,0); return; }
  const nf = prev || next || state.rootOrder[0]; setFocus(state,nf, state.nodes[nf].text.length);
}

function mergeNextSiblingIntoCurrent(state, id){ const n=state.nodes[id]; if(n.children.length) return; const next = nextSiblingId(state,id); if(!next) return; const m = state.nodes[next]; n.text += m.text; m.children.forEach(cid=>{ state.nodes[cid].parentId=id; n.children.push(cid); }); const sibs=siblingsRef(state,id); sibs.splice(sibs.indexOf(next),1); delete state.nodes[next]; setFocus(state,id,n.text.length); }

// UI rendering and events
const app = document.getElementById('app');
let state = initialState();

// Read CSS tokens once with safe fallbacks
function cssNumberVar(name, fallback) {
  const v = getComputedStyle(document.documentElement).getPropertyValue(name);
  const n = parseInt(v, 10);
  return Number.isFinite(n) ? n : fallback;
}
const ROOT_LEFT_PAD = cssNumberVar('--root-left-pad', 36);
const INDENT_STEP = cssNumberVar('--indent-step', 18);
const MENU_SIZE = cssNumberVar('--menu-size', 22);
const MENU_GAP = cssNumberVar('--menu-gap', 8);
const GLYPH_GAP = cssNumberVar('--glyph-gap', 8);
const BULLET_SIZE = cssNumberVar('--bullet-size', 8);

function render() {
  app.innerHTML = '';
  const container = document.createElement('div');
  (state.scopeRootId ? [state.scopeRootId] : state.rootOrder).forEach(id => renderNode(container, id, 0));
  app.appendChild(container);
}

function renderNode(parent, id, depth){
  const node = state.nodes[id];
  const row = document.createElement('div'); row.className='row'+(state.focusedId===id?' focused':''); row.dataset.id=id;
  const indentSpacer = document.createElement('div'); indentSpacer.className='indent';
  // base left pad + per-depth indent (computed once)
  indentSpacer.style.width = (ROOT_LEFT_PAD + depth * INDENT_STEP) + 'px';
  // Guides overlay (absolute inside row)
  const guides = document.createElement('div'); guides.className='guides';
  guides.style.width = (ROOT_LEFT_PAD + depth * INDENT_STEP) + 'px';
  // Compute ancestor chain up to parent, and whether each has a next sibling
  const ancestors = [];
  let cur = node.parentId;
  while (cur) { ancestors.unshift(cur); cur = state.nodes[cur].parentId; }
  // Draw vertical lines only; align with bullet centers at each ancestor depth
  const guideW = cssNumberVar('--guide-width', 2);
  ancestors.forEach((aid, i) => {
    const hasNext = !!nextSiblingId(state, aid);
    if (!hasNext) return; // only draw when continuation exists
    const v = document.createElement('div'); v.className='guide-line';
    // Align the vertical guide exactly with the ancestor bullet center.
    // i corresponds to ancestor depth (0 = root), so offset is i * INDENT_STEP.
    const bulletCenter = ROOT_LEFT_PAD + i * INDENT_STEP + MENU_SIZE + MENU_GAP + (BULLET_SIZE/2);
    v.style.left = (bulletCenter - guideW/2) + 'px';
    guides.appendChild(v);
  });
  const menu = document.createElement('div'); menu.className='menu';
  // three dots inside the menu chip
  for (let i=0;i<3;i++){ const d=document.createElement('span'); d.className='dot'; menu.appendChild(d); }
  const glyph = document.createElement('div'); glyph.className='glyph';
  const text = document.createElement('div'); text.className='text'; text.contentEditable='true'; text.textContent=node.text;
  row.appendChild(guides); row.appendChild(indentSpacer); row.appendChild(menu); row.appendChild(glyph); row.appendChild(text);
  parent.appendChild(row);

  // Clicking the glyph focuses this row for keyboard interactions
  glyph.addEventListener('click', () => { setFocus(state, id, 0); renderAndFocus(id, 0); });

  // On focus, just update state; do NOT re-render here or we will lose focus
  text.addEventListener('focus', () => { setFocus(state, id, node.text.length); });
  text.addEventListener('click', (e) => {
    const caret = getCaretOffset(text);
    setFocus(state, id, caret);
  });
  text.addEventListener('input', (e)=>{
    // sanitize to plain text only
    node.text = text.textContent || '';
    state.caret = getCaretOffset(text);
  });
  text.addEventListener('paste', (e)=>{
    e.preventDefault();
    const txt = (e.clipboardData || window.clipboardData).getData('text');
    const lines = txt.replace(/\r\n?/g,'\n').split('\n');
    if(lines.length===1){ insertTextAtCaret(text, lines[0]); node.text = text.textContent || ''; state.caret = getCaretOffset(text); return; }
    // multi-line: first line insert, rest create bullets after
    insertTextAtCaret(text, lines[0]); node.text = text.textContent || '';
    let after = id;
    for(let i=1;i<lines.length;i++){
      insertEmptySiblingAfter(state, after);
      const nid = state.focusedId; state.nodes[nid].text = lines[i]; after = nid;
    }
    renderAndFocus(state.focusedId, 0);
  });
  text.addEventListener('keydown', (e)=>{
    const isMac = navigator.platform.toUpperCase().indexOf('MAC')>=0;
    const mod = isMac ? e.metaKey : e.ctrlKey;
    if(e.key==='Enter'){
      e.preventDefault();
      const caret = getCaretOffset(text);
      if(caret>0 && caret<node.text.length){ splitAtCaret(state, id, caret); return renderAndFocus(state.focusedId, 0); }
      if(node.text.length>0){ insertEmptySiblingAfter(state, id); return renderAndFocus(state.focusedId, 0); }
      // empty
      if(node.parentId){ outdent(state, id); return renderAndFocus(id, 0); }
      insertEmptySiblingAfter(state, id); return renderAndFocus(state.focusedId, 0);
    }
    if(e.key==='Tab'){
      e.preventDefault();
      if(e.shiftKey){
        outdent(state, id);
        renderAndFocus(id, 0);
      } else {
        const prev = prevSiblingId(state, id);
        // Debug log to help diagnose indentation issues
        // console.debug('Tab indent', { id, prev, levelParent: state.nodes[id].parentId });
        indent(state, id);
        renderAndFocus(id, 0);
      }
    }
    if(mod && e.shiftKey && (e.key==='ArrowUp' || e.key==='Up')){ e.preventDefault(); moveUp(state, id); return renderAndFocus(id, 0); }
    if(mod && e.shiftKey && (e.key==='ArrowDown' || e.key==='Down')){ e.preventDefault(); moveDown(state, id); return renderAndFocus(id, 0); }
    if(e.key==='Backspace'){
      const caret = getCaretOffset(text);
      if(caret===0){
        if(node.text.length===0) { e.preventDefault(); deleteEmptyAtId(state, id); return renderAndFocus(state.focusedId, state.caret); }
        // backspace-at-start-with-text: no-op (do nothing special)
      }
    }
    if(e.key==='Delete'){
      const caret = getCaretOffset(text);
      if(caret===node.text.length){
        const next = nextSiblingId(state,id);
        if(next && node.children.length===0){ e.preventDefault(); mergeNextSiblingIntoCurrent(state, id); return renderAndFocus(id, state.caret); }
      }
    }
    if(e.key==='ArrowUp'){
      e.preventDefault(); const prev = prevVisibleId(state,id); if(prev){ setFocus(state,prev,0); return renderAndFocus(prev,0); }
    }
    if(e.key==='ArrowDown'){
      e.preventDefault(); const nxt = nextVisibleId(state,id); if(nxt){ setFocus(state,nxt,0); return renderAndFocus(nxt,0); }
    }
  });

  node.children.forEach(cid=> renderNode(parent, cid, depth+1));
}

function renderAndFocus(id, caret){ render(); const el = app.querySelector(`.row[data-id="${id}"] .text`); if(!el) return; setCaret(el, caret); el.focus(); }

function getCaretOffset(el){
  const sel = window.getSelection(); if(!sel || sel.rangeCount===0) return 0;
  const range = sel.getRangeAt(0);
  if(!el.contains(range.startContainer)) return 0;
  // assume single text node
  const pre = range.cloneRange(); pre.selectNodeContents(el); pre.setEnd(range.startContainer, range.startOffset);
  return (pre.toString()||'').length;
}
function setCaret(el, offset){
  el.focus();
  const sel = window.getSelection(); const range = document.createRange();
  const textNode = el.firstChild || el;
  const len = (el.textContent||'').length;
  const pos = Math.max(0, Math.min(offset, len));
  if(textNode.nodeType!==Node.TEXT_NODE){ el.textContent = el.textContent || ''; }
  range.setStart(el.firstChild||el, pos); range.collapse(true);
  sel.removeAllRanges(); sel.addRange(range);
}
function insertTextAtCaret(el, txt){
  const sel = window.getSelection(); if(!sel || sel.rangeCount===0) return;
  const r = sel.getRangeAt(0); r.deleteContents(); r.insertNode(document.createTextNode(txt)); r.collapse(false);
}

render();
