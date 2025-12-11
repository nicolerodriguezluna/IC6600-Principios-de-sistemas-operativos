// mmu.js — Clase principal DualMMU
// Simula dos algoritmos de reemplazo de páginas en paralelo: OPT (óptimo) y otro (FIFO, MRU, SC, RND).

import { createFIFOPolicy } from './fifo.js';
import { createMRUPolicy } from './mru.js';
import { createSecondChancePolicy } from './sc.js';
import { createRANDPolicy } from './rand.js';
import { createOPTPolicy } from './opt.js';

// ------------------------------------------------------------------
// Clase DualMMU — Controla la simulación paralela de dos MMUs
// ------------------------------------------------------------------
export class DualMMU {
  constructor({
    pageSizeKB = 4,        
    realPages = 100,       
    algorithm = "FIFO",    
    onSnapshot = null,     
    randSeed = null,       
    diskCostSec = 5        
  } = {}) {
    
    // Parámetros básicos
    this.pageSizeKB = pageSizeKB;
    this.realPages = realPages;
    this.algorithm = algorithm.toUpperCase();
    this.onSnapshot = typeof onSnapshot === 'function' ? onSnapshot : null;
    this.randSeed = randSeed;
    this.diskCostSec = diskCostSec;

    // Se crean dos simulaciones: OPT y el otro algoritmo
    this.opt = this._createSide("OPT");
    this.other = this._createSide(this.algorithm);

    // Reloj independiente para cada algoritmo
    this.time = { OPT: 0, OTHER: 0 };

    // Políticas de reemplazo
    this.optPolicy = null;
    this.otherPolicy = null;

    // Métricas globales por algoritmo
    this.metrics = {
      OPT: { hits: 0, misses: 0, thrash: 0, procRun: 0, procDone: 0 },
      OTHER: { hits: 0, misses: 0, thrash: 0, procRun: 0, procDone: 0 }
    };

    // Fragmentación interna acumulada
    this.fragKB = { OPT: 0, OTHER: 0 };
  }

  // ----------------------------------
  // Inicializa una “instancia” de MMU 
  // ----------------------------------
  _createSide(tag) {
    return {
      tag,                                 
      RAM: new Array(this.realPages).fill(null), 
      ptrMap: new Map(),                   
      pageCounter: 1,                      
      ptrCounter: 1,                       
      log: []                              
    };
  }

  // Carga la política óptima
  attachOPT(pageRefs) {
    this.optPolicy = createOPTPolicy(pageRefs);
  }

  // Cambia el algoritmo de comparación
  setOtherAlgorithm(name) {
    this.algorithm = String(name || 'FIFO').toUpperCase();
    this.other.tag = this.algorithm;
    this.otherPolicy = null;
  }

  // ------------------------------------------------------------------
  // Ejecuta una operación simultáneamente en ambos algoritmos
  // ------------------------------------------------------------------
  async execute(opName, args) {
    await this._runSide(this.opt, "OPT", opName, args);
    await this._runSide(this.other, "OTHER", opName, args);
  }

  // ------------------------------------
  // Utilidades de cálculo de RAM / VRAM
  // ------------------------------------
  _freeFrames(side) { return side.RAM.reduce((a, f) => a + (f ? 0 : 1), 0); }
  _ramKB(side) { return side.RAM.filter(Boolean).length * this.pageSizeKB; }
  _vramKB(side) {
    const all = Array.from(side.ptrMap.values()).flat();
    return all.filter(p => !p.inRAM).length * this.pageSizeKB;
  }

  // ------------------
  // Captura de estado
  // ------------------
  _snapshot(side, label) {
    const which = side.tag === "OPT" ? "OPT" : "OTHER";
    const m = this.metrics[which];

    // Conteo de páginas y estado de RAM
    const usedFrames = side.RAM.filter(Boolean).length;
    const freeFrames = this._freeFrames(side);
    const totalFrames = this.realPages;
    const allPages = Array.from(side.ptrMap.values()).flat();
    const pagesLoaded = usedFrames;
    const pagesUnloaded = Math.max(0, allPages.length - pagesLoaded);

    // Uso de memoria
    const RAM_KB = usedFrames * this.pageSizeKB;
    const VRAM_KB = pagesUnloaded * this.pageSizeKB;
    const RAM_PCT = totalFrames ? Math.round((usedFrames / totalFrames) * 100) : 0;
    const VRAM_PCT = allPages.length ? Math.round((pagesUnloaded / allPages.length) * 100) : 0;

    // Fragmentación interna acumulada
    const fragKB = Math.max(0, Math.round(this.fragKB[which]));

    // Tiempo de thrashing
    const thrTime = m.thrash;
    const totalSimTime = Math.max(1, this.time[which]);
    const thrPct = Math.round((thrTime / totalSimTime) * 100);

    // Procesos activos y finalizados
    const runDone = (() => {
      const alive = new Set();
      const all = new Set();
      for (const pages of side.ptrMap.values()) {
        if (pages.length) {
          all.add(pages[0].pid);
          if (pages.some(p => p.inRAM || p.daddr)) alive.add(pages[0].pid);
        }
      }
      return { run: alive.size, done: Math.max(0, all.size - alive.size) };
    })();

    // Objeto de snapshot (estado completo para la interfaz)
    const rec = {
      time: this.time[which],  
      op: label,               
      tag: side.tag,           
      RAM_KB,
      VRAM_KB,
      freeFrames,
      pagesLoaded,
      pagesUnloaded,
      fragKB,
      hits: m.hits,
      misses: m.misses,
      thrTime,
      thrPct,
      RAM_PCT,
      VRAM_PCT,
      procRun: runDone.run,
      procDone: runDone.done,
    };

    // Guarda snapshot en el log interno
    side.log.push(rec);

    // Enviar snapshot a la UI
    if (this.onSnapshot) {
      try {
        this.onSnapshot(rec, side);
      } catch (err) {
        console.error("[DualMMU] Error enviando snapshot:", err);
      }
    }
  }

  // ------------------------------------------------------------
  // Ejecuta una operación individual en una de las simulaciones
  // ------------------------------------------------------------
  async _runSide(side, which, opName, rawArgs) {
    const args = rawArgs.map(a => (a && a.trim ? a.trim() : a));
    switch (opName) {
      case 'new': this._opNew(side, Number(args[0]), Number(args[1])); break;
      case 'use': await this._opUse(side, which, Number(args[0])); break;
      case 'delete': this._opDelete(side, Number(args[0])); break;
      case 'kill': this._opKill(side, Number(args[0])); break;
      default: break;
    }
    this._snapshot(side, `${side.tag}:${opName}(${args.join(',')})`);
  }

  // ------------------------------------------------------------------
  // Crea la política de reemplazo para el algoritmo seleccionado
  // ------------------------------------------------------------------
  _createOtherPolicy() {
    console.log(`[MMU] Creando política para: ${this.algorithm}`);
    switch (this.algorithm) {
      case 'RND':
      case 'RAND':
      case 'RANDOM':
        console.log("Usando política RND");
        return createRANDPolicy({ seed: this.randSeed });

      case 'FIFO':
        console.log("Usando política FIFO");
        return createFIFOPolicy();

      case 'MRU':
        console.log("Usando política MRU");
        return createMRUPolicy();

      case 'SC':
      case 'SECOND_CHANCE':
        console.log("Usando política SC");
        return createSecondChancePolicy();

      default:
        return 0;
    }
  }

  // --------------------------------------------------
  // Crea un nuevo ptr y asigna páginas en RAM o disco
  // --------------------------------------------------
  _opNew(side, pid, sizeB) {
    const pages = Math.ceil(sizeB / (this.pageSizeKB * 1024));
    const ptr = side.ptrCounter++;
    const list = [];

    // Cálculo de fragmentación interna
    const usedKB = pages * this.pageSizeKB;
    const wastedKB = usedKB - (sizeB / 1024);

    side.ptrMap.set(ptr, list);
    if (!side.procFrag) side.procFrag = new Map();
    side.procFrag.set(ptr, wastedKB);

    // Crear cada página asociada al ptr
    for (let i = 0; i < pages; i++) {
      const id = side.pageCounter++;
      const page = { id, pid, ptr, inRAM: false, frame: null, laddr: id, maddr: null, daddr: null, loadedT: null };
      const frame = side.RAM.findIndex(f => f === null);

      if (frame !== -1) {
        // Si hay espacio en RAM, se carga directamente
        side.RAM[frame] = page;
        const which = side.tag === "OPT" ? "OPT" : "OTHER";
        Object.assign(page, { inRAM: true, frame, maddr: frame, loadedT: `${this.time[which]}s` });
      } else {
        // Si no hay espacio, se coloca en disco virtual
        page.daddr = `D${id}`;
      }

      list.push(page);
    }

    // Sumar fragmentación interna al total
    const which = side.tag === "OPT" ? "OPT" : "OTHER";
    this.fragKB[which] += wastedKB;

    this._bumpProcCounts();
  }

  // -----------------
  // Acceso a memoria 
  // -----------------
  async _opUse(side, which, ptr) {
    const pages = side.ptrMap.get(ptr) || [];
    if (!pages.length) return;

    // Inicializa política del otro algoritmo si aún no existe
    if (which === "OTHER" && !this.otherPolicy) {
      this.otherPolicy = this._createOtherPolicy();
    }

    for (const page of pages) {
      const res = (which === "OPT") ? this.optPolicy.ensure(side, page)
        : this.otherPolicy.ensure(side, page);

      const tKey = which; // Clave del reloj correspondiente

      if (res?.hit) {
        // Hit: la página ya estaba en RAM
        this.metrics[tKey].hits++;
        this.time[tKey] += 1; // +1s al reloj
      }
      if (res?.miss) {
        // Miss: hubo que traerla de disco
        this.metrics[tKey].misses++;
        this.metrics[tKey].thrash += this.diskCostSec; // +5s al thrashing
        this.time[tKey] += this.diskCostSec;           // +5s al reloj total
      }

      // Actualizar estado de direcciones
      if (page.inRAM) {
        page.maddr = page.frame;
        page.daddr = "";
        if (!page.loadedT) page.loadedT = `${this.time[tKey]}s`;
      } else {
        page.maddr = "";
        page.daddr = page.daddr || `D${page.id}`;
      }

      this._bumpProcCounts();
    }
  }

  // -------------------------------------------
  // Borra un ptr y todas sus páginas asociadas
  // -------------------------------------------
  _opDelete(side, ptr) {
    const list = side.ptrMap.get(ptr);
    if (!list) return;

    // Liberar frames y limpiar referencias
    for (const p of list) {
      if (p.inRAM && p.frame != null) side.RAM[p.frame] = null;
      p.inRAM = false; p.frame = null; p.maddr = ""; p.daddr = `D${p.id}`;
    }

    // Restar fragmentación interna del proceso eliminado
    if (side.procFrag && side.procFrag.has(ptr)) {
      const which = side.tag === "OPT" ? "OPT" : "OTHER";
      this.fragKB[which] -= side.procFrag.get(ptr);
      side.procFrag.delete(ptr);
    }

    side.ptrMap.delete(ptr);
    this._bumpProcCounts();
  }

  // ----------------------------------------------
  // Elimina todos los ptr y páginas de un proceso 
  // ----------------------------------------------
  _opKill(side, pid) {
    for (const [ptr, list] of side.ptrMap.entries()) {
      if (list.length && list[0].pid === pid) {
        for (const p of list) {
          if (p.inRAM && p.frame != null) side.RAM[p.frame] = null;
          p.inRAM = false; p.frame = null; p.maddr = ""; p.daddr = `D${p.id}`;
        }

        // Restar fragmentación interna del proceso
        if (side.procFrag && side.procFrag.has(ptr)) {
          const which = side.tag === "OPT" ? "OPT" : "OTHER";
          this.fragKB[which] -= side.procFrag.get(ptr);
          side.procFrag.delete(ptr);
        }

        side.ptrMap.delete(ptr);
      }
    }
    this._bumpProcCounts();
  }

  // -------------------------------------------------------
  // Actualiza contadores de procesos activos y finalizados
  // -------------------------------------------------------
  _bumpProcCounts() {
    const make = (side) => {
      const alive = new Set();
      const all = new Set();
      for (const pages of side.ptrMap.values()) {
        if (pages.length) {
          all.add(pages[0].pid);
          if (pages.some(p => p.inRAM || p.daddr)) alive.add(pages[0].pid);
        }
      }
      return { run: alive.size, done: Math.max(0, all.size - alive.size) };
    };
    const a = make(this.opt), b = make(this.other);
    this.metrics.OPT.procRun = a.run; this.metrics.OPT.procDone = a.done;
    this.metrics.OTHER.procRun = b.run; this.metrics.OTHER.procDone = b.done;
  }
}
