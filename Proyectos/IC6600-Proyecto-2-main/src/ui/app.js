const alg = document.getElementById('alg');
const out = document.getElementById('out');
document.getElementById('run').addEventListener('click', async () => {
  out.textContent = 'Ejecutando…';
  try {
    const r = await fetch(`/api/run?algorithm=${encodeURIComponent(alg.value)}`);
    const data = await r.json();
    out.textContent =
      `Algoritmo: ${data.algorithm}\n` +
      `Métricas:\n  OPT   -> hits:${data.metrics.OPT.hits}  misses:${data.metrics.OPT.misses}\n` +
      `  OTHER -> hits:${data.metrics.OTHER.hits} misses:${data.metrics.OTHER.misses}\n\n` +
      `Últimas muestras OPT:\n${JSON.stringify(data.sampleOpt, null, 2)}\n\n` +
      `Últimas muestras OTHER:\n${JSON.stringify(data.sampleOther, null, 2)}\n`;
  } catch (e) {
    out.textContent = 'Error: ' + e;
  }
});
