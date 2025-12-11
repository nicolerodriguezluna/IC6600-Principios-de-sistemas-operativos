// src/core/sc.js — Second Chance 
export function createSecondChancePolicy() {
  let nextIndex = 0;
  const referenceBits = new Map(); // pageId -> bit R (0 o 1)

  function ensure(side, pageObj) {
    const pageId = pageObj.id;

    // HIT
    if (pageObj.inRAM) {
      if (pageObj.inRAM && referenceBits.get(pageId) === 0) {
        referenceBits.set(pageId, 1);
      }
      pageObj.mark = 1;
      return { hit: true, pageId, frame: pageObj.frame };
    }

    // 🔹 MISS: buscar frame libre
    for (let i = 0; i < side.RAM.length; i++) {
      if (side.RAM[i] === null) {
        side.RAM[i] = pageObj;
        Object.assign(pageObj, { inRAM: true, frame: i, mark: 1 });
        if (pageObj.inRAM && referenceBits.get(pageId) === 0) {
          referenceBits.set(pageId, 1);
        }
        return { miss: true, load: true, pageId, frame: i };
      }
    }

    // Second Chance
    let victimFrame = null;
    let victimId = null;

    while (victimFrame === null) {
      const currentFrame = nextIndex;
      const currentPage = side.RAM[currentFrame];
      const currentId = currentPage.id;
      const bit = referenceBits.get(currentId) ?? 0;

      if (bit === 1) {
        // segunda oportunidad → se limpia el bit
        referenceBits.set(currentId, 0);
        currentPage.mark = 0;
      } else {
        // víctima encontrada
        victimFrame = currentFrame;
        victimId = currentId;
      }

      nextIndex = (nextIndex + 1) % side.RAM.length;
    }

    // Reemplazar víctima
    const victimObj = side.RAM[victimFrame];
    Object.assign(victimObj, { inRAM: false, frame: null, mark: 0 });
    referenceBits.delete(victimId);

    side.RAM[victimFrame] = pageObj;
    Object.assign(pageObj, { inRAM: true, frame: victimFrame, mark: 1 });
    referenceBits.set(pageId, 1);

    return { miss: true, repl: true, pageId, frame: victimFrame, victimId };
  }

  return { ensure };
}
