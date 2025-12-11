// public/app-simulate.js — Simulación OPT vs Algoritmo (FIFO/MRU/SC/RAND)
import { DualMMU } from '/src/core/mmu.js';
import { buildPageRefs } from '/src/core/refs_builder.js';

// -------- util ----------
const $ = (s) => document.querySelector(s);
const $$ = (s) => Array.from(document.querySelectorAll(s));
const setText = (id, val) => { const el = document.getElementById(id); if (el) el.textContent = String(val); };

// Config guardada en la pantalla anterior
const cfg = JSON.parse(sessionStorage.getItem('op2_config') || '{}');
const algoritmo = (cfg.algorithm || 'FIFO').toUpperCase();
const seed = cfg.randSeed != null ? Number(cfg.randSeed) : Number(cfg.seed) || 12345;
const opsText = String(cfg.opsText || '').trim();

// Pinta el algoritmo en el título derecho
const algTitleEl = document.getElementById('algTitle');
if (algTitleEl) algTitleEl.textContent = `MMU – ${algoritmo}`;

// -------- elementos ----------
const RAM_FRAMES = 100;            
const PAGE_KB = 4;

const elRamOpt = $('#ramOpt');
const elRamAlg = $('#ramAlg');
const elRamInfoOpt = $('#ramInfoOpt');
const elRamInfoAlg = $('#ramInfoAlg');
const elClock = $('#clock');

const btnStart = $('#btnStart');
const btnPause = $('#btnPause');
const btnStep = $('#btnStep');
const btnReset = $('#btnReset');
const speedEl = $('#speed');

const tbodyOPT = $('#tableOPT tbody');
const tbodyALG = $('#tableALG tbody');

// -------- UI helpers ----------
const pidColors = ['seg1','seg2','seg3','seg4','seg5','seg6','seg7','seg8','seg9','seg10','seg11','seg12','seg13','seg14'];
function pidToClass(pid) {
  const idx = ((pid || 0) - 1) % pidColors.length;
  return pidColors[idx];
}

// Helper de color directo por PID 
function colorVarForPid(pid) {
  const idx = ((pid || 0) - 1) % 7;
  const vars = ['--p1','--p2','--p3','--p4','--p5','--p6','--p7'];
  return getComputedStyle(document.documentElement).getPropertyValue(vars[idx]).trim();
}

function renderRamBar(el, side) {
  if (!el || !side?.RAM) return;
  el.innerHTML = '';
  const frag = document.createDocumentFragment();
  const total = RAM_FRAMES;

  const frameToPid = {};
  if (side.ptrMap && typeof side.ptrMap.values === 'function') {
    for (const list of side.ptrMap.values()) {
      for (const p of list) {
        if (p && p.inRAM && typeof p.frame === 'number') {
          frameToPid[p.frame] = p.pid;
        }
      }
    }
  }

  if (Object.keys(frameToPid).length === 0 && Array.isArray(side.RAM)) {
    for (let i = 0; i < side.RAM.length; i++) {
      const f = side.RAM[i];
      const pid = f?.pid ?? f?.page?.pid ?? f?.processId ?? 0;
      if (pid) frameToPid[i] = pid;
    }
  }

  for (let i = 0; i < total; i++) {
    const d = document.createElement('div');
    const pid = frameToPid[i] ?? 0;
    if (pid > 0) {
      const cls = pidToClass(pid);            
      const color = colorVarForPid(pid);        
      d.className = `cell used ${cls}`;
      d.style.backgroundColor = color;
    } else {
      d.className = 'cell';
      d.style.backgroundColor = '';             
    }
    frag.appendChild(d);
  }

  el.appendChild(frag);
}

function paintTable(tbody, rows) {
  if (!tbody) return;
  const tr = (r) => {
    // color base según el proceso (PID)
    const pidClass = pidToClass(r.pid);
    const markClass =
      r.mark === 'M'
        ? 'marked-mru'
        : r.mark === 1
        ? 'marked-sc1'
        : '';

    return `<tr class="${pidClass}">
      <td>${r.pageId}</td>
      <td>${r.pid}</td>
      <td>${r.loaded}</td>
      <td>${r.l}</td>
      <td>${r.m}</td>
      <td>${r.d}</td>
      <td>${r.t}</td>
      <td class="${markClass}">${r.mark || ''}</td>
    </tr>`;
  };
  tbody.innerHTML = rows.map(tr).join('');
}


function rowsFromSide(side) {
  const rows = [];
  if (side?.ptrMap && typeof side.ptrMap.values === 'function') {
    for (const list of side.ptrMap.values()) {
      for (const p of list) {
        rows.push({
          pageId: p.id, pid: p.pid,
          loaded: p.inRAM ? 'X' : '',
          l: p.laddr ?? p.id,
          m: p.inRAM ? (p.frame ?? '') : '',
          d: p.inRAM ? '' : (p.daddr ?? `D${p.id}`),
          t: p.loadedT ?? '',
          mark: p.mark ?? ''
        });
      }
    }
  }
  rows.sort((a, b) => (a.pid - b.pid) || (a.pageId - b.pageId));
  return rows;
}

// Métricas para tiles
function calcMetrics(side, pageKB, totalFrames, mmu, tag) {
  const usedFrames = Array.isArray(side?.RAM) ? side.RAM.filter(Boolean).length : 0;
  const allPages = Array.from(side.ptrMap.values()).flat();
  const pagesLoaded = allPages.filter(p => p.inRAM).length;
  const pagesUnloaded = allPages.length - pagesLoaded;
  const ramKB = usedFrames * pageKB;
  const ramPct = totalFrames ? Math.round((usedFrames / totalFrames) * 100) : 0;
  const vramKB = pagesUnloaded * pageKB;
  const vramPct = allPages.length ? Math.round((pagesUnloaded / allPages.length) * 100) : 0;

  // 🔹 Procesos en ejecución y finalizados desde mmu.metrics
  const procRun = mmu.metrics[tag].procRun;
  const procDone = mmu.metrics[tag].procDone;

  return { usedFrames, pagesLoaded, pagesUnloaded, ramKB, ramPct, vramKB, vramPct, procRun, procDone };
}


// -------- parse de operaciones (minúsculas) ----------
function parseOps(text) {
  const out = [];
  const re = /^\s*(new|use|delete|kill)\s*\(([^)]*)\)\s*$/i;
  for (const line of text.split(/\r?\n/)) {
    const ln = line.trim();
    if (!ln) continue;
    const m = ln.match(re);
    if (!m) continue;
    const op = m[1].toLowerCase();
    const args = m[2].split(',').map(s => s.trim()).filter(Boolean).map(Number);
    out.push({ op, args });
  }
  return out;
}
const ops = parseOps(opsText);

// -------- MMU ----------
const mmu = new DualMMU({
  pageSizeKB: PAGE_KB,
  realPages: RAM_FRAMES,
  algorithm: algoritmo,
  randSeed: seed,
  onSnapshot: (rec, side) => {
    const isOPT = side.tag === 'OPT';
    const metrics = calcMetrics(side, PAGE_KB, RAM_FRAMES, mmu, side.tag === 'OPT' ? 'OPT' : 'OTHER');

    // Barras + texto 82/100
    if (isOPT) {
      renderRamBar(elRamOpt, side);  
      if (elRamInfoOpt) elRamInfoOpt.textContent = `${metrics.usedFrames}/${RAM_FRAMES}`;
    } else {
      renderRamBar(elRamAlg, side);
      if (elRamInfoAlg) elRamInfoAlg.textContent = `${metrics.usedFrames}/${RAM_FRAMES}`;
    }


    // Hits / Misses
    if (isOPT) {
      if (rec?.hits != null) setText('optHits', rec.hits | 0);
      if (rec?.misses != null) setText('optMisses', rec.misses | 0);
    } else {
      if (rec?.hits != null) setText('algHits', rec.hits | 0);
      if (rec?.misses != null) setText('algMisses', rec.misses | 0);
    }

    // Tabla
    if (!window.lastRows) window.lastRows = { OPT: [], OTHER: [] };

    const rows = rowsFromSide(side);
    if (rows.length > 0) {
      // guarda la última tabla no vacía
      if (isOPT) window.lastRows.OPT = rows;
      else window.lastRows.OTHER = rows;
    }

    // usa la última disponible aunque esté vacía ahora
    if (isOPT) paintTable(tbodyOPT, window.lastRows.OPT);
    else paintTable(tbodyALG, window.lastRows.OTHER);

    // Tiles: Procesos
    if (isOPT) {
      setText('optProcRun', metrics.procRun);
      setText('optProcDone', metrics.procDone);
    } else {
      setText('algProcRun', metrics.procRun);
      setText('algProcDone', metrics.procDone);
    }

    // Tiles: Memoria
    if (isOPT) {
      setText('optRamKB', metrics.ramKB);
      setText('optRamPct', metrics.ramPct + '%');
      setText('optVRamKB', metrics.vramKB);
      setText('optVRamPct', metrics.vramPct + '%');
      setText('optLoaded', metrics.pagesLoaded);
      setText('optUnloaded', metrics.pagesUnloaded);
      setText('optFrag', `${rec.fragKB ?? 0} KB`);
    } else {
      setText('algRamKB', metrics.ramKB);
      setText('algRamPct', metrics.ramPct + '%');
      setText('algVRamKB', metrics.vramKB);
      setText('algVRamPct', metrics.vramPct + '%');
      setText('algLoaded', metrics.pagesLoaded);
      setText('algUnloaded', metrics.pagesUnloaded);
      setText('algFrag', `${rec.fragKB ?? 0} KB`);
    }

    // Reloj + tiempos (si no vienen en rec, avanzamos 1)
    const t = typeof rec?.time === 'number' ? rec.time : (Number(elClock?.textContent) || 0) + 1;
    setText('clock', t);
    setText(isOPT ? 'optTime' : 'algTime', `${t}s`);

    const thrT = rec?.thrTime ?? 0;
    const thrP = rec?.thrPct ?? 0;
    if (isOPT) {
      setText('optThrash', `${thrT}s`);
      const thrEl = document.getElementById('optThrPct');
      thrEl.textContent = `${thrP}%`;
      thrEl.className = thrP > 50 ? 'bad' : 'ok';  
    } else {
      setText('algThrash', `${thrT}s`);
      const thrEl = document.getElementById('algThrPct');
      thrEl.textContent = `${thrP}%`;
      thrEl.className = thrP > 50 ? 'bad' : 'ok';
    }
  }
});

// Conectar OPT con referencias futuras
try {
  const refs = buildPageRefs(opsText, { pageSizeKB: PAGE_KB });
  mmu.attachOPT(refs);
  mmu.setOtherAlgorithm(algoritmo);
} catch (e) {
  console.error('[simulate] Error preparando OPT:', e);
}

// -------- stepping ----------
let idx = 0;
let running = false;
let timer = 0;

// mmu.execute espera 'new'|'use'|'delete'|'kill' (minúsculas)
async function stepOnce() {
  if (idx >= ops.length) {
    running = false;
    window.clearTimeout(timer);
    console.log("Simulación finalizada.");
    return;
  }
  const { op, args } = ops[idx++];
  try {
    await mmu.execute(op, args);
  } catch (e) {
    console.error('Error ejecutando', op, args, e);
    running = false;
  }
}

function loop() {
  if (!running) return;
  const speed = Math.max(1, Math.min(20000, Number(speedEl?.value || 5)));
  stepOnce();
  timer = window.setTimeout(loop, 1000 / speed);
}

// -------- wire ----------
btnStart?.addEventListener('click', () => { if (!running) { running = true; loop(); } });
btnPause?.addEventListener('click', () => { running = false; window.clearTimeout(timer); });
btnStep?.addEventListener('click', () => { running = false; stepOnce(); });
btnReset?.addEventListener('click', () => { running = false; idx = 0; window.clearTimeout(timer); location.reload(); });

// -------- estado inicial ----------
elRamOpt.innerHTML = '<div class="cell"></div>'.repeat(RAM_FRAMES);
elRamAlg.innerHTML = '<div class="cell"></div>'.repeat(RAM_FRAMES);
setText('ramInfoOpt', `0/${RAM_FRAMES}`);
setText('ramInfoAlg', `0/${RAM_FRAMES}`);
if (!ops.length) btnStart?.setAttribute('disabled', 'true');