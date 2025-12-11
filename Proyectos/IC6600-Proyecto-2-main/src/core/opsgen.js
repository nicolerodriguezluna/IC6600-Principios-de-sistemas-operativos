#!/usr/bin/env node
//////////////////// RNG determinístico ////////////////////
function cyrb128(str) {
  let h1 = 1779033703, h2 = 3144134277, h3 = 1013904242, h4 = 2773480762;
  for (let i = 0; i < str.length; i++) {
    const k = str.charCodeAt(i);
    h1 = (h2 ^ Math.imul(h1 ^ k, 597399067)) >>> 0;
    h2 = (h3 ^ Math.imul(h2 ^ k, 2869860233)) >>> 0;
    h3 = (h4 ^ Math.imul(h3 ^ k, 951274213)) >>> 0;
    h4 = (h1 ^ Math.imul(h4 ^ k, 2716044179)) >>> 0;
  }
  h1 = (h3 ^ (h1 >>> 18)) >>> 0;
  h2 = (h4 ^ (h2 >>> 22)) >>> 0;
  h3 = (h1 ^ (h3 >>> 17)) >>> 0;
  h4 = (h2 ^ (h4 >>> 19)) >>> 0;
  return [(h1 ^ h2 ^ h3 ^ h4) >>> 0, h1>>>0, h2>>>0, h3>>>0];
}
function mulberry32(a) {
  return function() {
    let t = (a += 0x6D2B79F5) >>> 0;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
class SeededRandom {
  constructor(seed) {
    const s = (typeof seed === 'number') ? seed >>> 0 : cyrb128(String(seed))[0];
    this._rng = mulberry32(s);
  }
  int(min, max) { return Math.floor(this._rng() * (max - min + 1)) + min; }
  pick(arr) { return arr[this.int(0, arr.length - 1)]; }
  float() { return this._rng(); }
}

//////////////////// Config ////////////////////
const DEFAULT_TOLERANCE = 0;
const DEFAULT_ALLOW_OVERSHOOT = true;

//////////////////// Generador principal ////////////////////
export function generateOperations({
  P = 10,
  N = 500,
  seed = 12345,
  sizeMin = 32,
  sizeMax = 16384,
  tolerance = DEFAULT_TOLERANCE,
  allowOvershoot = DEFAULT_ALLOW_OVERSHOOT,
} = {}) {
  if (P <= 0) throw new Error('P debe ser > 0');
  if (N <= 0) throw new Error('N debe ser > 0');
  if (!allowOvershoot && N < P) {
    throw new Error(`N (= ${N}) debe ser >= P (= ${P}) sin overshoot.`);
  }

  const R = new SeededRandom(seed);

  const minLen = Math.max(P, Math.floor(N * (1 - tolerance)));
  const maxLen = allowOvershoot ? Math.floor(N * (1 + tolerance)) : N;

  // *** Control GLOBAL entre kills ***
  const MIN_GLOBAL = Math.max(1, Math.floor(N / P)); // p.ej. 50 si N=500, P=10
  let sinceLastKill = 0;
  let hazard = 0;                 // prob acumulada
  const HAZARD_INC = 1 / MIN_GLOBAL; // crece para promediar ~MIN_GLOBAL entre kills

  // Estado de procesos
  const procs = Array.from({ length: P }, (_, i) => ({
    pid: i + 1,
    alive: true,
    ptrs: new Set(),
  }));

  const ptrOwner = new Map();
  let nextPtrId = 1;

  const ops = [];
  const aliveCount = () => procs.filter(p => p.alive).length;

  function addOp(s) { ops.push(s); }
  function randomSize() {
    return Math.floor((R.int(sizeMin, sizeMax) + R.int(sizeMin, sizeMax)) / 2);
  }
  function pickAliveProc() {
    const living = procs.filter(p => p.alive);
    return living.length ? living[R.int(0, living.length - 1)] : null;
  }

  while (ops.length < maxLen && aliveCount() > 0) {
    const alive = aliveCount();

    // Reserva: debe quedar espacio para 1 kill por vivo
    const roomForNonKill = maxLen - ops.length - alive;
    const mustOnlyKill = roomForNonKill <= 0;

    let didKill = false;

    // Intento de kill: o por obligación, o si ya pasamos el mínimo global y el hazard "pega"
    const canTryKill = mustOnlyKill || (sinceLastKill >= MIN_GLOBAL && (hazard += HAZARD_INC, R.float() < Math.min(1, hazard)));

    if (canTryKill) {
      // elegir víctima aleatoria entre vivos
      const victim = pickAliveProc();
      // evita matar al último proceso si aún no alcanzamos minLen (salvo obligación)
      if (victim && !(alive === 1 && ops.length < minLen && !mustOnlyKill)) {
        victim.alive = false;
        for (const ptr of victim.ptrs) ptrOwner.delete(ptr);
        victim.ptrs.clear();
        addOp(`kill(${victim.pid})`);
        sinceLastKill = 0;
        hazard = 0;
        didKill = true;
      }
    }

    if (didKill) continue;

    // No-kill
    const p = pickAliveProc();
    if (!p) break;

    const canUseDelete = p.ptrs.size > 0;
    // Menú sencillo con más peso a new y use
    const choices = ['new','new','new','new'];
    if (canUseDelete) choices.push('use','use','use','delete');
    const op = R.pick(choices);

    if (op === 'new') {
      const size = randomSize();
      const ptr = nextPtrId++;
      p.ptrs.add(ptr);
      ptrOwner.set(ptr, p.pid);
      addOp(`new(${p.pid}, ${size})`);
      sinceLastKill++;
      continue;
    }
    if (op === 'use' && canUseDelete) {
      const ptrArr = Array.from(p.ptrs);
      const ptr = ptrArr[R.int(0, ptrArr.length - 1)];
      addOp(`use(${ptr})`);
      sinceLastKill++;
      continue;
    }
    if (op === 'delete' && canUseDelete) {
      const ptrArr = Array.from(p.ptrs);
      const ptr = ptrArr[R.int(0, ptrArr.length - 1)];
      p.ptrs.delete(ptr);
      ptrOwner.delete(ptr);
      addOp(`delete(${ptr})`);
      sinceLastKill++;
      continue;
    }

    // fallback: new
    const size = randomSize();
    const ptr = nextPtrId++;
    p.ptrs.add(ptr);
    ptrOwner.set(ptr, p.pid);
    addOp(`new(${p.pid}, ${size})`);
    sinceLastKill++;
  }

  // Cierre: matar lo que quede si hay espacio
  for (const p of procs) {
    if (p.alive && ops.length < maxLen) {
      p.alive = false;
      for (const ptr of p.ptrs) ptrOwner.delete(ptr);
      p.ptrs.clear();
      addOp(`kill(${p.pid})`);
    }
  }

  return ops.join('\n') + '\n';
}

//////////////////// CLI ////////////////////
if (typeof module !== 'undefined' && require?.main === module) {
  const args = Object.fromEntries(
    process.argv.slice(2).map(kv => {
      const [k, v] = kv.replace(/^--/, '').split('=');
      return [k, v ?? true];
    })
  );
  const P = Number(args.P ?? 10);
  const N = Number(args.N ?? 500);
  const seed = args.seed ?? 12345;
  const sizeMin = args.sizeMin ? Number(args.sizeMin) : 32;
  const sizeMax = args.sizeMax ? Number(args.sizeMax) : 16384;
  const tolerance = args.tolerance ? Number(args.tolerance) : DEFAULT_TOLERANCE;
  const allowOvershoot = (String(args.allowOvershoot ?? DEFAULT_ALLOW_OVERSHOOT).toLowerCase() === 'true'
                          || String(args.allowOvershoot).trim() === '1');

  const txt = generateOperations({
    P, N, seed, sizeMin, sizeMax,
    tolerance,
    allowOvershoot
  });
  process.stdout.write(txt);
} else {
  if (typeof window !== 'undefined') {
    window.generateOperations = generateOperations;
  }
}

// --- Protección y compatibilidad ---
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { generateOperations };
}
