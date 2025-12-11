// src/core/mru.js 
export function createMRUPolicy() {
  const order = []; // guarda el orden de uso (más reciente al inicio)

  function getPageById(side, id) {
    for (const pages of side.ptrMap.values()) {
      for (const p of pages) if (p.id === id) return p;
    }
    return null;
  }

  function ensure(side, pageObj) {
    const pageId = pageObj.id;

    // HIT 
    if (pageObj.inRAM) {
      const idx = order.indexOf(pageId);
      if (idx !== -1) order.splice(idx, 1);
      order.unshift(pageId);

      // quitar marca anterior
      for (const pid of order.slice(1)) {
        const pg = getPageById(side, pid);
        if (pg) pg.mark = '';
      }

      pageObj.mark = 'M';
      return { hit: true, pageId, frame: pageObj.frame };
    }

    // Frame libre
    const free = side.RAM.findIndex(f => f === null);
    if (free !== -1) {
      side.RAM[free] = pageObj;
      Object.assign(pageObj, { inRAM: true, frame: free, mark: 'M' });
      order.unshift(pageId);
      return { miss: true, load: true, pageId, frame: free };
    }

    // sacar la más reciente (orden[0])
    const victimId = order.shift();
    const victimPg = getPageById(side, victimId);
    const frame = victimPg?.frame ?? 0;

    if (victimPg && victimPg.inRAM) {
      Object.assign(victimPg, { inRAM: false, frame: null, mark: '' });
      side.RAM[frame] = null;
    }

    // cargar nueva página
    side.RAM[frame] = pageObj;
    Object.assign(pageObj, { inRAM: true, frame, mark: 'M' });
    order.unshift(pageId);

    // actualizar marcas
    for (const pid of order.slice(1)) {
      const pg = getPageById(side, pid);
      if (pg) pg.mark = '';
    }

    return { miss: true, repl: true, pageId, frame, victimId };
  }

  return { ensure };
}
