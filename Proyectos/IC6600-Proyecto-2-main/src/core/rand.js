// rand.js — Política de reemplazo de páginas Random (RND)
// Implementa un algoritmo de reemplazo aleatorio utilizando un generador
// pseudoaleatorio determinista (Xorshift32) para asegurar reproducibilidad.

export function createRANDPolicy({ seed = 0xC0FFEE } = {}) {
  // ------------------------------------------------------------------
  // Generador pseudoaleatorio (Xorshift32)
  // ------------------------------------------------------------------
  // Se usa una semilla inicial para producir resultados reproducibles
  // entre simulaciones.
  let s = (seed >>> 0) || 0xC0FFEE;

  // Función de número aleatorio entre 0 y 1
  // Utiliza operaciones bit a bit (Xorshift) para evitar dependencias externas.
  const rnd = () => {
    s ^= s << 13; s >>>= 0;
    s ^= s >> 17; s >>>= 0;
    s ^= s << 5;  s >>>= 0;
    return (s >>> 0) / 4294967296; // Devuelve un número flotante entre 0 y 1
  };

  // ------------------------------------------------------------------
  // Selección aleatoria de víctima (cuando no hay marcos libres)
  // ------------------------------------------------------------------
  const pickVictimFrame = (side) => {
    const occ = [];
    // Se crea un arreglo con los índices de los marcos actualmente ocupados.
    for (let i = 0; i < side.RAM.length; i++) if (side.RAM[i]) occ.push(i);

    // Si no hay páginas cargadas (RAM vacía), se retorna null.
    if (!occ.length) return null;

    // Selecciona un marco aleatorio dentro de los ocupados.
    return occ[Math.floor(rnd() * occ.length)];
  };

  // ------------------------------------------------------------------
  // Función principal: ensure(side, pageObj)
  // ------------------------------------------------------------------
  // Asegura que la página indicada esté en memoria real (RAM).
  // Si ya está en RAM → hit.
  // Si no está y hay espacio libre → carga directa.
  // Si no hay espacio → reemplazo aleatorio.
  function ensure(side, pageObj) {
    const pageId = pageObj.id;

    // Caso 1: la página ya está en RAM → hit
    if (pageObj.inRAM) {
      return { hit: true, pageId, frame: pageObj.frame };
    }

    // Caso 2: hay un marco libre → carga directa
    const free = side.RAM.findIndex(f => f === null);
    if (free !== -1) {
      side.RAM[free] = pageObj;
      Object.assign(pageObj, { inRAM: true, frame: free });
      return { miss: true, load: true, pageId, frame: free };
    }

    // Caso 3: memoria llena → reemplazo aleatorio
    const frame = pickVictimFrame(side) ?? 0;
    const victimObj = side.RAM[frame];
    const victimId = victimObj?.id;

    // Se expulsa la página víctima
    if (victimObj && victimObj.inRAM) {
      side.RAM[frame] = null;
      Object.assign(victimObj, { inRAM: false, frame: null });
    }

    // Se carga la nueva página en el marco liberado
    side.RAM[frame] = pageObj;
    Object.assign(pageObj, { inRAM: true, frame });

    // Devuelve un registro de reemplazo
    return { miss: true, repl: true, pageId, frame, victimId };
  }

  // Retorna el objeto de política con su método principal
  return { ensure };
}
