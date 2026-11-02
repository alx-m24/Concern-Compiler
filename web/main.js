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

// Keys must match the Department names in main.cpp exactly.
// title = row-1 text in the sheet (full form wording), fill = row-1 colour. Fills are placeholders: adjust freely.
const DEPT_STYLE = {
  "Technology & Online Services": { title: "Technology & Online Services",                          fill: "FFF2CC" },
  "Student Services & Admin":     { title: "Student Services and Administrative Matters",           fill: "D9D2E9" },
  "Campus Facilities":            { title: "Campus Facilities",                                     fill: "F4CCCC" },
  "Food, Retail & Commercial":    { title: "Food, Retail & Commercial Services",                    fill: "EAD1DC" },
  "Student Activities & Life":    { title: "Student Activities & Campus Life",                      fill: "D0E0E3" },
  "International Students":       { title: "International Students Matters",                        fill: "FFE599" },
  "Fees & Financial Assistance":  { title: "Fees & Financial Assistance",                           fill: "FCE5CD" },
  "Academics":                    { title: "Academics",                                             fill: "FFE599" },
  "Accessibility & Inclusion":    { title: "Accessibility & Inclusion",                             fill: "D9EAD3" },
  "Others":                       { title: "Others",                                                fill: "EFEFEF" },
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

/* ------------------------------------------------------------------ */
/* DOM helpers                                                         */
/* ------------------------------------------------------------------ */

const $ = id => document.getElementById(id);
const status = $('status');

// Everything coming from students or the LLM goes through textContent, never innerHTML.
function el(tag, props = {}, ...kids) {
  const n = document.createElement(tag);
  for (const [k, v] of Object.entries(props)) {
    if (k === 'class') n.className = v;
    else if (k === 'text') n.textContent = v;
    else n.setAttribute(k, v);
  }
  n.append(...kids);
  return n;
}

function setStatus(msg, isError = false) {
  status.textContent = msg;
  status.classList.toggle('error', isError);
}

function formatDuration(ms) {
  const total = Math.max(0, Math.round(ms / 1000));
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  const ss = String(s).padStart(2, '0');
  return h > 0 ? `${h}:${String(m).padStart(2, '0')}:${ss}` : `${m}:${ss}`;
}

/* ------------------------------------------------------------------ */
/* Globals written by C++ (EM_JS): compilationProgress, neuronPercentage */
/* ------------------------------------------------------------------ */

window.compilationProgress = 0;
window.neuronPercentage = 0;

let compileStartedAt = null;   // performance.now() while compiling, otherwise null
let lastElapsedMs = 0;

function setBar(id, pct) {
  const bar = $(id);
  bar.firstElementChild.style.width = `${pct}%`;
  bar.setAttribute('aria-valuenow', pct.toFixed(0));
}

function updateNeuronBar() {
  const raw = Number(window.neuronPercentage);
  if (!Number.isFinite(raw)) return;
  const pct = Math.min(Math.max(raw, 0), 100);

  setBar('neurons', pct);
  $('neuronsText').textContent = `${pct.toFixed(1)}%`;
  $('neurons').classList.toggle('warn', pct >= 75 && pct < 95);
  $('neurons').classList.toggle('danger', pct >= 95);
}

function updateProgressBar() {
  const pct = Math.min(Math.max(Number(window.compilationProgress) || 0, 0), 100);
  setBar('progress', pct);
  $('progressText').textContent = `${pct.toFixed(0)}%`;

  if (compileStartedAt === null) return;     // not compiling: keep the final numbers on screen

  const elapsed = performance.now() - compileStartedAt;
  lastElapsedMs = elapsed;
  $('elapsed').textContent = formatDuration(elapsed);

  // simple proportion: time per percent so far * percent still to go
  $('eta').textContent = pct > 0 ? formatDuration(elapsed * (100 - pct) / pct) : 'estimating…';
}

setInterval(() => {            // always on: neurons change on init too, not only while compiling
  updateNeuronBar();
  updateProgressBar();
}, 250);

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

const resultBox = $('result');
let lastRows = null;
let activeDept = 'All';

function renderEmpty(msg) {
  resultBox.replaceChildren(el('div', { class: 'empty', text: msg }));
}

function renderResults() {
  if (!lastRows?.length) { renderEmpty('No concerns in the database yet.'); return; }

  const counts = new Map();
  for (const r of lastRows) counts.set(r.category, (counts.get(r.category) ?? 0) + 1);
  if (activeDept !== 'All' && !counts.has(activeDept)) activeDept = 'All';

  const chip = (label, n) => {
    const b = el('button', { class: 'chip', type: 'button', 'aria-pressed': String(activeDept === label) }, label, el('b', { text: String(n) }));
    b.addEventListener('click', () => { activeDept = label; renderResults(); });
    return b;
  };
  const filters = el('div', { class: 'filters' }, chip('All', lastRows.length));
  for (const dept of [...counts.keys()].sort()) filters.append(chip(dept, counts.get(dept)));

  const shown = lastRows
    .filter(r => activeDept === 'All' || r.category === activeDept)
    .sort((a, b) => (a.category === b.category ? 0 : a.category < b.category ? -1 : 1) || b.count - a.count);

  const showDeptCol = activeDept === 'All';
  const head = el('tr', {},
    el('th', { class: 'num', text: 'No' }),
    ...(showDeptCol ? [el('th', { text: 'Department' })] : []),
    el('th', { text: 'Concern' }),
    el('th', { class: 'num', text: 'Count' }),
    el('th', { text: 'References' }));

  const body = el('tbody');
  shown.forEach((r, i) => body.append(el('tr', {},
    el('td', { class: 'num', text: String(i + 1) }),
    ...(showDeptCol ? [el('td', { class: 'dept', text: r.category })] : []),
    el('td', { text: r.rewritten }),
    el('td', { class: 'num', text: String(r.count) }),
    el('td', { class: 'refs', text: safeParse(r.references).join('\n') }))));

  resultBox.replaceChildren(
    filters,
    el('div', { class: 'table-wrap' }, el('table', {}, el('thead', {}, head), body)));
}

// Preview of the loaded input file (first rows only)
function renderInputPreview(json) {
  let data;
  try { data = JSON.parse(json); } catch { renderEmpty('Could not read the file.'); return; }

  const records = data.records ?? [];
  const columns = data.columns ?? [];
  const concernCol = ["What is your concern(s)?", "What is the student's concern"].find(c => columns.includes(c))
                     ?? columns[columns.length - 1];

  const body = el('tbody');
  records.slice(0, 8).forEach((r, i) => body.append(el('tr', {},
    el('td', { class: 'num', text: String(i + 2) }),            // spreadsheet row number
    el('td', { text: String(r[concernCol] ?? '') }))));

  resultBox.replaceChildren(
    el('p', { class: 'hint', text: `Preview of the first ${Math.min(8, records.length)} of ${records.length} rows.` }),
    el('div', { class: 'table-wrap' }, el('table', {},
      el('thead', {}, el('tr', {}, el('th', { class: 'num', text: 'Row' }), el('th', { text: concernCol ?? 'Concern' }))),
      body)));
}

function show(json) {
  let v;
  try { v = JSON.parse(json); } catch (e) { console.error("show: result was not JSON", e); return; }
  if (!Array.isArray(v)) return;
  lastRows = v;
  $('download').disabled = v.length === 0;
  renderResults();
}

/* ------------------------------------------------------------------ */
/* File reading                                                        */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/* Startup                                                             */
/* ------------------------------------------------------------------ */

const Module = await createModule();
const compiler = new Module.ConcernCompiler("Concern Compiler");

try {
  const ok = await compiler.init();
  console.log("init resolved:", ok);
  updateNeuronBar();
  if (ok) {
    $('input').disabled = false;
    setStatus('Ready. Choose a responses file to begin.');
  } else {
    setStatus('Startup failed. The daily neuron cap may be reached, or the database or LLM is unreachable (see console).', true);
  }
} catch (e) {
  console.error("init threw:", e);
  setStatus('Startup failed: ' + e, true);
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

// run an async action with Compile locked, then restore its previous state
async function withCompileLocked(fn) {
  const wasDisabled = $('compile').disabled;
  $('compile').disabled = true;
  try { return await fn(); }
  finally { $('compile').disabled = wasDisabled; }
}

// 1. new concerns file uploaded
$('input').addEventListener('change', async e => {
  const file = e.target.files[0];
  $('compile').disabled = true;
  if (!file) return;

  const tabs = await readTabs(file, false);
  const n = compiler.inputData(tabs[0].csv, file.name);   // first tab only
  setStatus(`Loaded ${n} concerns from ${file.name}.`);
  $('compile').disabled = false;

  renderInputPreview(compiler.getInputConcerns());
});

$('reset_db').addEventListener('click', () => withCompileLocked(async () => {
  if (!confirm('This deletes every saved concern, keyword and submission. Continue?')) return;
  await compiler.clearData();
  lastRows = null;
  $('download').disabled = true;
  renderEmpty('Database cleared.');
  setStatus('Database cleared.');
}));

$('import').addEventListener('click', () => withCompileLocked(async () => {
  setStatus('Loading saved results…');
  show(await compiler.ImportExisting());
  setStatus(`Loaded ${lastRows?.length ?? 0} saved concerns.`);
}));

$('download').addEventListener('click', () => {
  if (!lastRows?.length) { setStatus('Nothing to export yet.'); return; }
  exportConcernsXlsx(lastRows, `concerns-${new Date().toISOString().slice(0, 10)}.xlsx`);
});

$('compile').addEventListener('click', async () => {
  $('compile').disabled = true;
  $('download').disabled = true;

  window.compilationProgress = 0;
  $('eta').textContent = 'estimating…';
  compileStartedAt = performance.now();
  setStatus('Compiling…');

  try {
    show(await compiler.compile());
    const stopped = Number(window.compilationProgress) < 100;
    setStatus(stopped
      ? 'Stopped early: the daily neuron cap was reached. Compile again tomorrow to finish.'
      : `Done in ${formatDuration(performance.now() - compileStartedAt)}.`, stopped);
  } catch (e) {
    console.error("compile threw:", e);
    setStatus('Compile failed: ' + e, true);
  } finally {
    lastElapsedMs = performance.now() - compileStartedAt;
    compileStartedAt = null;
    updateProgressBar();
    $('elapsed').textContent = formatDuration(lastElapsedMs);
    $('eta').textContent = Number(window.compilationProgress) >= 100 ? '0:00' : '–';
    updateNeuronBar();
    $('compile').disabled = false;                       // a file is loaded if we got here
    $('download').disabled = !lastRows?.length;
  }
});
