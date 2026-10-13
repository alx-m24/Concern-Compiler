import createModule from './app.js';
const XLSX = window.XLSX;

const safeParse = (s) => { try { const v = JSON.parse(s); return Array.isArray(v) ? v : []; } catch { return []; } };

function sheetName(name, used) {
  let base = name.replace(/[\\/?*\[\]:]/g, " ").trim().slice(0, 31) || "Sheet";
  let out = base, n = 2;
  while (used.has(out.toLowerCase())) { const sfx = ` (${n++})`; out = base.slice(0, 31 - sfx.length) + sfx; }
  used.add(out.toLowerCase());
  return out;
}

// row-1 title and fill colour per department (taken from your file)
const DEPT_STYLE = {
  "Academics":                   { title: "Academics",                              fill: "FFE599" },
  "Facilities":                  { title: "Facilities Management",                  fill: "F4CCCC" },
  "EHS":                         { title: "Environment, Health & Safety",           fill: "FCE5CD" },
  "ICT":                         { title: "Information & Communication Technology", fill: "FFF2CC" },
  "International Office":        { title: "International Office",                   fill: "FFE599" },
  "Fees and Finance":            { title: "Fees and Finance",                       fill: "FFE599" },
  "Campus Security":             { title: "Campus Security",                        fill: "FCE5CD" },
  "Timetabling":                 { title: "Timetabling Department",                 fill: "D9D2E9" },
  "Student Development":         { title: "Student Development",                    fill: "EAD1DC" },
  "(SyopzMall) Commercial Area": { title: "Syopz Mall / Commercial Area",           fill: "EAD1DC" },
};

const BLACK  = { rgb: "000000" };
const medium = { style: "medium", color: BLACK };
const thin   = { style: "thin",   color: BLACK };
const box    = (b) => ({ top: b, bottom: b, left: b, right: b });

const titleStyle = (fill) => ({
  font: { name: "Times New Roman", sz: 14, bold: true },
  fill: { patternType: "solid", fgColor: { rgb: fill } },
  alignment: { horizontal: "center", vertical: "center" },
  border: box(medium),
});
const headerStyle = {
  font: { name: "Times New Roman", sz: 14, bold: true },
  fill: { patternType: "solid", fgColor: { rgb: "C9DAF8" } },
  alignment: { horizontal: "center", vertical: "center" },
  border: box(medium),
};
const bodyStyle = {
  font: { name: "Times New Roman", sz: 12 },
  alignment: { horizontal: "center", vertical: "center", wrapText: true },
  border: box(thin),
};

export function exportConcernsXlsx(json, fileName = "concerns.xlsx") {
  const rows = typeof json === "string" ? JSON.parse(json) : json;
  if (!rows.length) return;

  const byDept = new Map();
  for (const r of rows) {
    if (!byDept.has(r.category)) byDept.set(r.category, []);
    byDept.get(r.category).push(r);
  }

  const wb = XLSX.utils.book_new();
  const used = new Set();
  const COLS = 4;

  for (const dept of [...byDept.keys()].sort()) {
    const list = byDept.get(dept).sort((a, b) => b.count - a.count);
    const look = DEPT_STYLE[dept] ?? { title: dept, fill: "FFE599" };

    const aoa = [
      [look.title, "", "", ""],
      ["No", "Concern", "Count", "References"],
      ...list.map((r, i) => [i + 1, r.rewritten, r.count, safeParse(r.references).join("\n")]),
    ];
    const ws = XLSX.utils.aoa_to_sheet(aoa);

    for (let R = 0; R < aoa.length; R++) {
      for (let C = 0; C < COLS; C++) {
        const addr = XLSX.utils.encode_cell({ r: R, c: C });
        if (!ws[addr]) ws[addr] = { t: "s", v: "" };      // empty/merged cells still need a border
        ws[addr].s = R === 0 ? titleStyle(look.fill) : R === 1 ? headerStyle : bodyStyle;
      }
    }

    ws["!merges"] = [{ s: { r: 0, c: 0 }, e: { r: 0, c: COLS - 1 } }];   // merged title row
    ws["!cols"]   = [{ wch: 10 }, { wch: 100 }, { wch: 10 }, { wch: 28 }];
    XLSX.utils.book_append_sheet(wb, ws, sheetName(dept, used));
  }

  XLSX.writeFile(wb, fileName);
}

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

let lastRows = null;

function show(json) {
    $('result').textContent = json;
    try {
        const v = JSON.parse(json);
        if (Array.isArray(v)) {
            lastRows = v;
            $('download').disabled = v.length === 0;
        }
    } catch (e) {
        console.error("show: result was not JSON", e);
    }
}

// run an async action with Compile locked, then restore its previous state
async function withCompileLocked(fn) {
    const wasDisabled = $('compile').disabled;
    $('compile').disabled = true;
    try { return await fn(); }
    finally { $('compile').disabled = wasDisabled; }
}

$('reset_db').addEventListener('click', () => withCompileLocked(async () => {
    await compiler.clearData();
    lastRows = null;
    $('download').disabled = true;
    $('result').textContent = '[]';
    console.log("JS: Successfully cleared database data");
}));

$('import').addEventListener('click', () => withCompileLocked(async () => {
    show(await compiler.ImportExisting());
}));

$('download').addEventListener('click', () => {
    if (!lastRows?.length) { status.textContent = 'Nothing to export yet.'; return; }
    exportConcernsXlsx(lastRows, `concerns-${new Date().toISOString().slice(0, 10)}.xlsx`);
});

$('compile').addEventListener('click', async () => {
    $('compile').disabled = true;
    $('download').disabled = true;

    const timer = setInterval(() => {
        updateProgressBar();
        updateNeuronBar();
    }, 250);

    try {
        show(await compiler.compile());
    } finally {
        clearInterval(timer);
        updateProgressBar();
        updateNeuronBar();
        $('compile').disabled = false;                       // a file is loaded if we got here
        $('download').disabled = !lastRows?.length;
    }
});
