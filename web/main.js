import createModule from './app.js';

const $ = id => document.getElementById(id);
const status = $('status');

const Module = await createModule();

const compiler = new Module.ConcernCompiler("Concern Compiler");

console.log("calling init");
let ok;
try {
    ok = await compiler.init();
    console.log("init resolved:", ok, typeof ok);
    if (ok) {
        updateNeuronBar();
        const file_input = $('input');
        file_input.disabled = false;
    
       status.textContent = 'WASM ready.';
    }
    else {
        status.textContent = 'Init failed.';
    }
} catch (e) {
    console.error("init threw:", e);
}

// Helper functions
async function readTabs(file, hasTitleRow) {
  if (/\.csv$/i.test(file.name)) {
    return [{ name: file.name.replace(/\.csv$/i, ''), title: '', csv: await file.text() }];
  }

  const wb = XLSX.read(await file.arrayBuffer());
  return wb.SheetNames.map(name => {
    const ws = wb.Sheets[name];
    let title = name;
    let sheet = ws;

    if (hasTitleRow && ws['!ref']) {
      if (ws['A1'] && ws['A1'].v != null) title = String(ws['A1'].v).trim();
      const range = XLSX.utils.decode_range(ws['!ref']);
      range.s.r = 1;                                          // start reading at row 2
      sheet = { ...ws, '!ref': XLSX.utils.encode_range(range) };
    }

    return {
      name,
      title,
      csv: XLSX.utils.sheet_to_csv(sheet, { blankrows: false, strip: true }),
    };
  });
}

function updateNeuronBar() {
    const raw = Number(compiler.getNeuronPercentage());

    if (!Number.isFinite(raw)) {
        return;
    }

    const pct = Math.min(Math.max(raw, 0), 100);

    $('neurons').value = pct;
    $('neuronsText').textContent = `${pct.toFixed(1)}%`;
}

window.compilationProgress = 0;

function updateProgressBar() {
    const pct = Math.min(
        Math.max(Number(window.compilationProgress), 0),
        100
    );

    $('progress').value = pct;
    $('progressText').textContent = `${pct.toFixed(0)}%`;
}

// 1. new concerns file uploaded
$('input').addEventListener('change', async e => {
    const file = e.target.files[0];
    $('compile').disabled = true;
    if (!file) return;

    const tabs = await readTabs(file, false);
    const n = compiler.inputData(tabs[0].csv, file.name);   // first tab only
    status.textContent = `Loaded ${n} concerns from ${file.name}.`;
    $('compile').disabled = false;

    $('result').textContent = compiler.getInputConcerns();
});

$('reset_db').addEventListener('click', async () => {
    $('compile').disabled = true;

    try {
        const ok = await compiler.clearData();
        console.log("JS: Successfully cleared database data");
    } finally {
        $('compile').disabled = false;
    }
});

$('compile').addEventListener('click', async () => {
    $('compile').disabled = true;

    const timer = setInterval(() => {
        updateProgressBar();
        updateNeuronBar();
    }, 250);

    try {
        $('result').textContent = await compiler.compile();
    } finally {
        clearInterval(timer);
        updateProgressBar();   // final refresh so the bars show the end state
        updateNeuronBar();
        $('compile').disabled = false;
    }
});
