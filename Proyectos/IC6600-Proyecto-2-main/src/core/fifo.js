// fifo-simple.js
export function createFIFOPolicy() {
  let nextIndex = 0;
  
  function ensure(side, pageObj) {
    const pageId = pageObj.id;

    // HIT
    if (pageObj.inRAM) {
      return { hit: true, pageId, frame: pageObj.frame };
    }

    // MISS: Buscar frame libre
    for (let i = 0; i < side.RAM.length; i++) {
      if (side.RAM[i] === null) {
        side.RAM[i] = pageObj;
        Object.assign(pageObj, { inRAM: true, frame: i });
        return { miss: true, load: true, pageId, frame: i };
      }
    }

    // REPLACE: FIFO
    const victimFrame = nextIndex;
    const victimObj = side.RAM[victimFrame];
    const victimId = victimObj.id; 

    // Reemplazar
    Object.assign(victimObj, { inRAM: false, frame: null });
    side.RAM[victimFrame] = pageObj;
    Object.assign(pageObj, { inRAM: true, frame: victimFrame });

    // Actualizar siguiente índice
    nextIndex = (nextIndex + 1) % side.RAM.length;

    return { miss: true, repl: true, pageId, frame: victimFrame, victimId };
  }

  return { ensure };
}