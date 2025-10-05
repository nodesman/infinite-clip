// BulletCanvas DOM core — framework-agnostic editor
export class BulletCanvas {
  constructor(container, options = {}) {
    this.container = container;
    this.opts = { autofocus: !!options.autofocus, showBreadcrumbs: options.showBreadcrumbs !== false, tokens: options.tokens || {}, value: options.value };
    this.state = options.value || this.initialState();
    this.rootEl = document.createElement('div');
    this.rootEl.className = 'bc-root';
    this.container.appendChild(this.rootEl);
    // Apply tokens
    Object.entries(this.opts.tokens).forEach(([k,v]) => { this.rootEl.style.setProperty(`--${k.replace(/[A-Z]/g,m=>'-'+m.toLowerCase())}`, String(v)); });
    this.render();
    if (this.opts.autofocus) {
      const id = this.state.focusedId;
      this.focusNode(id, this.state.caret || 0);
    }
  }

  // Public API
  getState() { return structuredClone(this.state); }
  setState(s) { this.state = structuredClone(s); this.render(); this.emit('change', { state: this.getState() }); }
  setScopeRoot(id) { this.state.scopeRootId = id || null; this.render(); this.emit('drilldown', { id }); }
  focusNode(id, caret = 0) { this.state.focusedId = id; this.state.caret = caret; const el = this.rootEl.querySelector(`.row[data-id="${id}"] .text, .heading[data-id="${id}"] .heading-text`); if (el) { this.setCaret(el, caret); el.focus(); } this.emit('focus', { id, caret }); }
  applyCommand(cmd) { /* reserved for future direct command API */ }
  destroy() { this.rootEl.remove(); }

  emit(type, detail) { this.container.dispatchEvent(new CustomEvent(type, { detail })); }

  // State helpers
  initialState() { const id = this.newId(); return { nodes: { [id]: { id, parentId: '', text: '', children: [] } }, rootOrder: [id], focusedId: id, caret: 0, scopeRootId: null }; }
  newId() { this._idc = (this._idc||0)+1; return `j${this._idc}`; }

  // Tree and list helpers (ported from demo)
  siblingsRef(id) { const p = this.state.nodes[id].parentId; return p ? this.state.nodes[p].children : this.state.rootOrder; }
  prevSiblingId(id){ const a=this.siblingsRef(id); const i=a.indexOf(id); return i>0?a[i-1]:''; }
  nextSiblingId(id){ const a=this.siblingsRef(id); const i=a.indexOf(id); return (i>=0&&i+1<a.length)?a[i+1]:''; }
  visibleOrderIds(){ const out=[]; const dfs=(id)=>{ out.push(id); this.state.nodes[id].children.forEach(dfs);}; if (this.state.scopeRootId&&this.state.nodes[this.state.scopeRootId]){ dfs(this.state.scopeRootId); return out;} this.state.rootOrder.forEach(dfs); return out; }
  prevVisibleId(id){ const a=this.visibleOrderIds(); const i=a.indexOf(id); return i>0?a[i-1]:''; }
  nextVisibleId(id){ const a=this.visibleOrderIds(); const i=a.indexOf(id); return (i>=0&&i+1<a.length)?a[i+1]:''; }

  setFocus(id, caret=0){ this.state.focusedId=id; this.state.caret=caret; }
  insertEmptySiblingAfter(id){ const node=this.state.nodes[id]; const nid=this.newId(); this.state.nodes[nid]={id:nid,parentId:node.parentId,text:'',children:[]}; const sibs=this.siblingsRef(id); const idx=sibs.indexOf(id); sibs.splice(idx+1,0,nid); this.setFocus(nid,0); return nid; }
  appendEmptyChild(parentId){ const nid=this.newId(); this.state.nodes[nid]={id:nid,parentId,text:'',children:[]}; this.state.nodes[parentId].children.push(nid); this.setFocus(nid,0); return nid; }
  ensureTrailingEmptyChild(parentId){ const ch=this.state.nodes[parentId].children; if(ch.length===0) return this.appendEmptyChild(parentId); const lastId=ch[ch.length-1]; if((this.state.nodes[lastId].text||'').length>0) return this.appendEmptyChild(parentId); return null; }

  indent(id){ const prev=this.prevSiblingId(id); if(!prev) return; const sibs=this.siblingsRef(id); sibs.splice(sibs.indexOf(id),1); this.state.nodes[id].parentId=prev; this.state.nodes[prev].children.push(id); }
  outdent(id){ const n=this.state.nodes[id]; if(!n.parentId) return; const p=n.parentId, gp=this.state.nodes[p].parentId; const pc=this.state.nodes[p].children; pc.splice(pc.indexOf(id),1); if(!gp){ const i=this.state.rootOrder.indexOf(p); this.state.rootOrder.splice(i+1,0,id); n.parentId=''; } else { const gpc=this.state.nodes[gp].children; const i=gpc.indexOf(p); gpc.splice(i+1,0,id); n.parentId=gp; } }
  moveUp(id){ const n=this.state.nodes[id]; const sibs=this.siblingsRef(id); const i=sibs.indexOf(id); if(i>0){ [sibs[i-1],sibs[i]]=[sibs[i],sibs[i-1]]; return; } if(!n.parentId) return; const p=n.parentId, gp=this.state.nodes[p].parentId; this.state.nodes[p].children.splice(this.state.nodes[p].children.indexOf(id),1); if(!gp){ const j=this.state.rootOrder.indexOf(p); this.state.rootOrder.splice(j,0,id); n.parentId=''; } else { const gpc=this.state.nodes[gp].children; const j=gpc.indexOf(p); gpc.splice(j,0,id); n.parentId=gp; } }
  moveDown(id){ const n=this.state.nodes[id]; const sibs=this.siblingsRef(id); const i=sibs.indexOf(id); if(i+1<sibs.length){ [sibs[i],sibs[i+1]]=[sibs[i+1],sibs[i]]; return; } if(!n.parentId) return; const p=n.parentId, gp=this.state.nodes[p].parentId; this.state.nodes[p].children.splice(this.state.nodes[p].children.indexOf(id),1); if(!gp){ const j=this.state.rootOrder.indexOf(p); this.state.rootOrder.splice(j+1,0,id); n.parentId=''; } else { const gpc=this.state.nodes[gp].children; const j=gpc.indexOf(p); gpc.splice(j+1,0,id); n.parentId=gp; } }
  splitAtCaret(id, caret){ const node=this.state.nodes[id]; caret=Math.max(0,Math.min(caret,node.text.length)); const nid=this.newId(); this.state.nodes[nid]={id:nid,parentId:node.parentId,text:node.text.slice(caret),children:node.children.slice()}; node.children.forEach(cid=>this.state.nodes[cid].parentId=nid); node.children=[]; node.text=node.text.slice(0,caret); const sibs=this.siblingsRef(id); const idx=sibs.indexOf(id); sibs.splice(idx+1,0,nid); this.setFocus(nid,0); }
  deleteEmptyAtId(id){ const n=this.state.nodes[id]; if(!n||n.text.length>0) return; if(n.children.length) return; if(this.state.scopeRootId && id===this.state.scopeRootId){ this.state.nodes[id].text=''; this.setFocus(id,0); return; } const isRoot=!n.parentId; if(isRoot && this.state.rootOrder.length===1){ n.text=''; this.setFocus(id,0); return; } const prev=this.prevVisibleId(id), next=this.nextVisibleId(id); const parent=this.state.nodes[id].parentId; const sibs=this.siblingsRef(id); sibs.splice(sibs.indexOf(id),1); delete this.state.nodes[id]; if(this.state.rootOrder.length===0){ const rid=this.newId(); this.state.nodes[rid]={id:rid,parentId:'',text:'',children:[]}; this.state.rootOrder=[rid]; this.setFocus(rid,0); return; } if(this.state.scopeRootId && prev===this.state.scopeRootId){ if(next){ this.setFocus(next,0); return;} if(parent && parent===this.state.scopeRootId && this.state.nodes[parent].children.length===0){ const nid=this.appendEmptyChild(parent); this.setFocus(nid,0); return; } }
  const nf=prev||next||this.state.rootOrder[0]; this.setFocus(nf,(this.state.nodes[nf].text||'').length); }
  mergeNextSiblingIntoCurrent(id){ const n=this.state.nodes[id]; if(n.children.length) return; const next=this.nextSiblingId(id); if(!next) return; const m=this.state.nodes[next]; n.text+=m.text; m.children.forEach(cid=>{ this.state.nodes[cid].parentId=id; n.children.push(cid); }); const sibs=this.siblingsRef(id); sibs.splice(sibs.indexOf(next),1); delete this.state.nodes[next]; this.setFocus(id,(n.text||'').length); }

  // Rendering (ported)
  render(){ const app=this.rootEl; app.innerHTML=''; const container=document.createElement('div'); container.className='bc-container';
    // breadcrumbs
    if(this.opts.showBreadcrumbs){ const bc=document.createElement('div'); bc.className='breadcrumbs'; const makeCrumb=(id,label,isCurrent=false)=>{ const span=document.createElement('span'); span.className='crumb'+(isCurrent?' current':''); span.textContent=label; if(!isCurrent){ span.addEventListener('click',()=>{ this.state.scopeRootId=id||null; this.render(); this.emit('drilldown',{id});}); } return span; }; const chain=[]; if(this.state.scopeRootId&&this.state.nodes[this.state.scopeRootId]){ let c=this.state.scopeRootId; const tmp=[]; while(c){ tmp.unshift(c); c=this.state.nodes[c].parentId; } chain.push(...tmp);} if(!this.state.scopeRootId){ bc.appendChild(makeCrumb('', 'All', true)); } else { bc.appendChild(makeCrumb('', 'All', false)); const sep=()=>{ const s=document.createElement('span'); s.className='crumb-sep'; s.textContent='›'; return s; }; bc.appendChild(sep()); chain.forEach((id,idx)=>{ const isLast=idx===chain.length-1; const text=(this.state.nodes[id].text||'').trim()||'Untitled'; bc.appendChild(makeCrumb(id,text,isLast)); if(!isLast) bc.appendChild(sep()); }); } container.appendChild(bc); }
    // heading + list
    if(this.state.scopeRootId && this.state.nodes[this.state.scopeRootId]){ const hid=this.state.scopeRootId; const heading=document.createElement('div'); heading.className='heading'; heading.dataset.id=hid; const indentSpacer=document.createElement('div'); indentSpacer.className='indent'; const glyph=document.createElement('div'); glyph.className='glyph'; const htext=document.createElement('div'); htext.className='heading-text'; htext.contentEditable='true'; htext.textContent=this.state.nodes[hid].text||''; heading.appendChild(indentSpacer); heading.appendChild(glyph); heading.appendChild(htext); container.appendChild(heading); htext.addEventListener('keydown',(e)=>{ if(e.key==='Enter'){ e.preventDefault(); const nid=this.appendEmptyChild(hid); this.render(); this.focusNode(nid,0);} }); htext.addEventListener('input',()=>{ this.state.nodes[hid].text=htext.textContent||''; this.emit('change',{state:this.getState()});}); this.state.nodes[hid].children.forEach(id=> this.renderNode(container,id,0)); }
    else { this.state.rootOrder.forEach(id=> this.renderNode(container,id,0)); }
    app.appendChild(container); requestAnimationFrame(()=>this.drawGuides());
  }

  renderNode(parent,id,depth){ const node=this.state.nodes[id]; const row=document.createElement('div'); row.className='row'+(this.state.focusedId===id?' focused':''); row.dataset.id=id; const indentSpacer=document.createElement('div'); indentSpacer.className='indent'; indentSpacer.style.width = (this.cssNum('--root-left-pad',44) + depth * this.cssNum('--indent-step',40)) + 'px'; const guides=document.createElement('div'); guides.className='guides'; const menu=document.createElement('div'); menu.className='menu'; for(let i=0;i<3;i++){ const d=document.createElement('span'); d.className='dot'; menu.appendChild(d);} const glyph=document.createElement('div'); glyph.className='glyph'; const text=document.createElement('div'); text.className='text'; text.contentEditable='true'; text.textContent=node.text;
    row.appendChild(guides); row.appendChild(indentSpacer); row.appendChild(menu); row.appendChild(glyph); row.appendChild(text); parent.appendChild(row);
    // events
    glyph.addEventListener('click',()=>{ this.state.scopeRootId=id; const nid=this.ensureTrailingEmptyChild(id); this.render(); if(nid) this.focusNode(nid,0); this.emit('drilldown',{id}); });
    text.addEventListener('focus', ()=>{ this.setFocus(id, (node.text||'').length); this.emit('focus',{id, caret:this.state.caret}); });
    text.addEventListener('click', ()=>{ const caret=this.getCaretOffset(text); this.setFocus(id, caret); this.emit('focus',{id, caret}); });
    text.addEventListener('input', ()=>{ node.text=text.textContent||''; this.state.caret=this.getCaretOffset(text); this.emit('change',{state:this.getState()}); });
    text.addEventListener('paste',(e)=>{ e.preventDefault(); const txt=(e.clipboardData||window.clipboardData).getData('text'); const lines=txt.replace(/\r\n?/g,'\n').split('\n'); if(lines.length===1){ this.insertTextAtCaret(text, lines[0]); node.text=text.textContent||''; this.state.caret=this.getCaretOffset(text); this.emit('change',{state:this.getState()}); return;} this.insertTextAtCaret(text,lines[0]); node.text=text.textContent||''; let after=id; for(let i=1;i<lines.length;i++){ const nid=this.insertEmptySiblingAfter(after); this.state.nodes[nid].text=lines[i]; after=nid; } this.render(); this.focusNode(after,(this.state.nodes[after].text||'').length); });
    text.addEventListener('keydown',(e)=>{ const isMac=/Mac/i.test(navigator.platform); const mod=isMac?e.metaKey:e.ctrlKey; if(e.key==='Enter'){ e.preventDefault(); const caret=this.getCaretOffset(text); if(caret>0 && caret<node.text.length){ this.splitAtCaret(id, caret); this.render(); this.focusNode(this.state.focusedId,0); return;} if(node.text.length>0){ const nid=this.insertEmptySiblingAfter(id); this.render(); this.focusNode(nid,0); return;} if(node.parentId){ this.outdent(id); this.render(); this.focusNode(id,0); return;} const nid=this.insertEmptySiblingAfter(id); this.render(); this.focusNode(nid,0); return; }
      if(e.key==='Tab'){ e.preventDefault(); if(e.shiftKey){ this.outdent(id); this.render(); this.focusNode(id,0);} else { this.indent(id); this.render(); this.focusNode(id,0);} return; }
      if(mod && e.shiftKey && (e.key==='ArrowUp'||e.key==='Up')){ e.preventDefault(); this.moveUp(id); this.render(); this.focusNode(id,0); return; }
      if(mod && e.shiftKey && (e.key==='ArrowDown'||e.key==='Down')){ e.preventDefault(); this.moveDown(id); this.render(); this.focusNode(id,0); return; }
      if(e.key==='Backspace'){ const caret=this.getCaretOffset(text); if(caret===0){ if(node.text.length===0){ e.preventDefault(); this.deleteEmptyAtId(id); this.render(); this.focusNode(this.state.focusedId,this.state.caret); } } return; }
      if(e.key==='Delete'){ const caret=this.getCaretOffset(text); if(caret===node.text.length){ const next=this.nextSiblingId(id); if(next && node.children.length===0){ e.preventDefault(); this.mergeNextSiblingIntoCurrent(id); this.render(); this.focusNode(id,this.state.caret);} } return; }
      if(e.key==='ArrowUp'){ e.preventDefault(); const prev=this.prevVisibleId(id); if(prev){ this.setFocus(prev,0); this.render(); this.focusNode(prev,0);} return; }
      if(e.key==='ArrowDown'){ e.preventDefault(); const nxt=this.nextVisibleId(id); if(nxt){ this.setFocus(nxt,0); this.render(); this.focusNode(nxt,0);} return; }
    });
  }

  drawGuides(){ const rows=this.rootEl.querySelectorAll('.row'); rows.forEach(row=>{ const id=row.getAttribute('data-id'); const guides=row.querySelector('.guides'); if(!guides) return; guides.innerHTML=''; const rowRect=row.getBoundingClientRect(); guides.style.width=Math.ceil(rowRect.width)+'px'; const chain=[]; let ac=this.state.nodes[id]?this.state.nodes[id].parentId:''; while(ac){ chain.unshift(ac); ac=this.state.nodes[ac].parentId; } chain.forEach(aid=>{ if(!this.nextSiblingId(aid)) return; const ag=this.rootEl.querySelector(`.row[data-id="${aid}"] .glyph, .heading[data-id="${aid}"] .glyph`); if(!ag) return; const gr=ag.getBoundingClientRect(); const centerX=gr.left+gr.width/2 - rowRect.left + this.cssNum('--guide-offset',0); const v=document.createElement('div'); v.className='guide-line'; const w=this.cssNum('--guide-width',1); v.style.left=(Math.round(centerX - w/2))+'px'; guides.appendChild(v); }); }); }

  // caret helpers
  getCaretOffset(el){ const sel=window.getSelection(); if(!sel||sel.rangeCount===0) return 0; const range=sel.getRangeAt(0); if(!el.contains(range.startContainer)) return 0; const pre=range.cloneRange(); pre.selectNodeContents(el); pre.setEnd(range.startContainer, range.startOffset); return (pre.toString()||'').length; }
  setCaret(el, offset){ el.focus(); const sel=window.getSelection(); const range=document.createRange(); const len=(el.textContent||'').length; const pos=Math.max(0,Math.min(offset,len)); if(!el.firstChild){ el.textContent = el.textContent || ''; } range.setStart(el.firstChild||el,pos); range.collapse(true); sel.removeAllRanges(); sel.addRange(range); }
  insertTextAtCaret(el, txt){ const sel=window.getSelection(); if(!sel||sel.rangeCount===0) return; const r=sel.getRangeAt(0); r.deleteContents(); r.insertNode(document.createTextNode(txt)); r.collapse(false); }

  cssNum(varName, fallback){ const v=getComputedStyle(this.rootEl).getPropertyValue(varName); const n=parseInt(v,10); return Number.isFinite(n)?n:fallback; }
}

