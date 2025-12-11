// refs_builder.js (ESM)
export function parseOps(opsText){
  return opsText
    .split('\n').map(s=>s.trim())
    .filter(s=>s && !s.startsWith('//'))
    .map(line=>{
      const m = line.match(/^(\w+)\(([^)]*)\)\s*$/);
      if (!m) return { raw: line, cmd:'noop', args:[] };
      const cmd = m[1];
      const args = m[2].split(',').map(a=>a.trim()).filter(Boolean);
      return { raw: line, cmd, args };
    });
}

export function buildPageRefs(opsText, { pageSizeKB = 4 } = {}){
  const ops = parseOps(opsText);
  const refs = [];
  let pageCounter = 1;
  let ptrCounter = 1;
  const ptrMap = new Map(); // ptr -> [{id,pid,ptr}]

  for (const {cmd, args} of ops){
    if (cmd === 'new'){
      const pid = Number(args[0]);
      const sizeB = Number(args[1]);
      const pages = Math.ceil(sizeB / (pageSizeKB*1024));
      const ptr = ptrCounter++;
      const list = [];
      for (let i=0;i<pages;i++){
        list.push({ id: pageCounter++, pid, ptr });
      }
      ptrMap.set(ptr, list);
    } else if (cmd === 'use'){
      const ptr = Number(args[0]);
      const list = ptrMap.get(ptr) || [];
      for (const pg of list) refs.push(pg.id);
    } else if (cmd === 'delete'){
      const ptr = Number(args[0]);
      ptrMap.delete(ptr);
    } else if (cmd === 'kill'){
      const pid = Number(args[0]);
      for (const [k,v] of Array.from(ptrMap.entries())){
        if (v.length && v[0].pid === pid) ptrMap.delete(k);
      }
    }
  }
  return refs;
}
