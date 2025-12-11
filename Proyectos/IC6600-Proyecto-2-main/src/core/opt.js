// opt.js — Política Óptima (OPT)
// Simula el algoritmo teórico de reemplazo de páginas Óptimo,
// que siempre reemplaza la página cuya próxima utilización está más lejos en el futuro.

// ------------------------------------------------
// Construye la estructura de referencias futuras 
// ------------------------------------------------
function buildFutureStacks(refs) {
  const m = new Map();
  for (let i = refs.length - 1; i >= 0; i--) {
    const id = refs[i];
    if (!m.has(id)) m.set(id, []);
    m.get(id).push(i); 
  }
  return m;
}

// ------------------------------------------------------------------
// Crea la política Óptima a partir de la secuencia de referencias
// ------------------------------------------------------------------
export function createOPTPolicy(pageRefs) {
  const future = buildFutureStacks(pageRefs); 
  const framesMap = new Map();                
  let refIdx = 0;                            

  // ----------------------------------------------------------
  // Busca una página por ID dentro de la estructura de la MMU
  // ----------------------------------------------------------
  const getPageById = (side, id) => {
    for (const pages of side.ptrMap.values()) {
      for (const pg of pages) if (pg.id === id) return pg;
    }
    return null;
  };

  // ------------------------------------------------------------------
  // Calcula la próxima posición donde se usará una página específica
  // ------------------------------------------------------------------
  const nextUseAfterNow = (id) => {
    const st = future.get(id);
    if (st && st[st.length - 1] === refIdx) st.pop();
    return (st && st.length) ? st[st.length - 1] : Infinity; // Si no se vuelve a usar → Infinity
  };

  // ----------------------------------------------------
  // Elige la página que se usará más lejos en el futuro
  // ----------------------------------------------------
  const chooseVictim = () => {
    let victim = null, far = -1;
    for (const [pg, nx] of framesMap) {
      if (nx > far) { far = nx; victim = pg; }
    }
    return victim;
  };

  // ------------------------------------------------------
  // Asegura que la página solicitada esté en memoria real 
  // ------------------------------------------------------
  function ensure(side, pageObj) {
    const pageId = pageObj.id;
    const nxt = nextUseAfterNow(pageId); // Próxima vez que será usada

    // Caso 1: la página ya está cargada → hit
    if (pageObj.inRAM) {
      framesMap.set(pageId, nxt); // Actualiza su próxima referencia
      refIdx++;                   // Avanza el contador global de referencias
      return { hit: true, pageId, frame: pageObj.frame };
    }

    // Caso 2: hay espacio libre → se carga directamente
    const free = side.RAM.findIndex(f => f === null);
    if (free !== -1) {
      side.RAM[free] = pageObj;
      Object.assign(pageObj, { inRAM: true, frame: free });
      framesMap.set(pageId, nxt);
      refIdx++;
      return { miss: true, load: true, pageId, frame: free };
    }

    // Caso 3: memoria llena → reemplaza la página más lejana en el futuro
    const victimId = chooseVictim();
    const victimObj = getPageById(side, victimId);
    const frame = victimObj?.frame ?? 0;

    // Expulsa la víctima de RAM
    if (victimObj && victimObj.inRAM) {
      side.RAM[frame] = null;
      Object.assign(victimObj, { inRAM: false, frame: null });
      framesMap.delete(victimId);
    }

    // Carga la nueva página en el marco liberado
    side.RAM[frame] = pageObj;
    Object.assign(pageObj, { inRAM: true, frame });

    // Actualiza el mapa de referencias y el índice global
    framesMap.set(pageId, nxt);
    refIdx++;

    // Retorna el resultado del acceso
    return { miss: true, repl: true, pageId, frame, victimId };
  }

  // Devuelve la interfaz pública de la política Óptima
  return { ensure };
}