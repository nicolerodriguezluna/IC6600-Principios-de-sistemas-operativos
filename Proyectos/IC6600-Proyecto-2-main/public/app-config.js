// Estado local
let loadedOpsText = "";
let generatedOpsText = "";

// Elementos base
const algEl = document.getElementById("algorithm");
const seedEl = document.getElementById("seed");
const randSeedLabel = document.getElementById("randSeedLabel");
const randSeedEl = document.getElementById("randSeed");
const opsFileEl = document.getElementById("opsFile");
const PEl = document.getElementById("P");
const NEl = document.getElementById("N");

// Radios
const modeUploadEl = document.getElementById("modeUpload");
const modeGenerateEl = document.getElementById("modeGenerate");
const uploadControls = document.getElementById("uploadControls");
const generateControls = document.getElementById("generateControls");

// Botones
const btnClearFile = document.getElementById("btnClearFile");
const btnGenerate = document.getElementById("btnGenerate");
const btnDownload = document.getElementById("btnDownload");
const btnContinue = document.getElementById("btnContinue");

// --- Funciones auxiliares ---
function setMode(mode) {
  if (mode === "upload") {
    uploadControls.style.opacity = "1";
    uploadControls.querySelectorAll("input,button").forEach(e => e.disabled = false);
    generateControls.style.opacity = "0.5";
    generateControls.querySelectorAll("input,button,select").forEach(e => e.disabled = true);
    generatedOpsText = "";
  } else {
    generateControls.style.opacity = "1";
    generateControls.querySelectorAll("input,button,select").forEach(e => e.disabled = false);
    uploadControls.style.opacity = "0.5";
    uploadControls.querySelectorAll("input,button").forEach(e => e.disabled = true);
    opsFileEl.value = "";
    loadedOpsText = "";
  }
}

algEl.addEventListener("change", () => {
  const alg = algEl.value.toUpperCase();
  if (alg === "RND") {
    randSeedLabel.style.display = "flex";
  } else {
    randSeedLabel.style.display = "none";
    randSeedEl.value = ""; // limpiar si se cambia
  }
});

// Inicial
setMode("upload");

// Radios listener
modeUploadEl.addEventListener("change", () => setMode("upload"));
modeGenerateEl.addEventListener("change", () => setMode("generate"));

// Limpiar selección de archivo
btnClearFile.addEventListener("click", () => {
  opsFileEl.value = "";
  loadedOpsText = "";
});

// Leer archivo cargado
opsFileEl.addEventListener("change", async () => {
  const f = opsFileEl.files?.[0];
  if (!f) return;
  loadedOpsText = await f.text();
});

// Generar archivo real con opsgen.js
btnGenerate.addEventListener("click", () => {
  const P = Number(PEl.value);
  const N = Number(NEl.value);
  const seed = Number(seedEl.value);
  try {
    const txt = window.generateOperations({ P, N, seed });
    generatedOpsText = txt;
    loadedOpsText = "";
    alert(`Archivo generado con ${P} procesos y ${N} operaciones`);
  } catch (err) {
    console.error(err);
    alert("Error al generar el archivo.");
  }
});

// Descargar el archivo generado
btnDownload.addEventListener("click", () => {
  if (!generatedOpsText) {
    alert("Primero genera un archivo antes de descargarlo.");
    return;
  }
  const blob = new Blob([generatedOpsText], { type: "text/plain" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "operaciones.txt";
  a.click();
  URL.revokeObjectURL(a.href);
});

// Continuar
btnContinue.addEventListener("click", () => {
  const mode = modeUploadEl.checked ? "upload" : "generate";
  const opsText = mode === "upload" ? loadedOpsText : generatedOpsText;
  const algorithm = algEl.value.toUpperCase();
  const baseSeed = Number(seedEl.value);
  const randSeed = Number(randSeedEl.value);

  // Validaciones
  if (!opsText) {
    alert("No hay archivo cargado ni generado.");
    return;
  }

  if (algorithm === "RND" && (!randSeedEl.value || isNaN(randSeed))) {
    alert("Debes ingresar una semilla para el algoritmo Random.");
    return;
  }

  // Guardar configuración completa
  sessionStorage.setItem("op2_config", JSON.stringify({
    algorithm,
    seed: baseSeed,
    randSeed: algorithm === "RND" ? randSeed : null,
    opsText
  }));

  window.location.href = "./simulate.html";
});