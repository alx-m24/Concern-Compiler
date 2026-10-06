import createModule from './app.js';

const $ = id => document.getElementById(id);
const status = $('status');

const Module = await createModule();
status.textContent = 'WASM ready.';

let compiled = null;   // { tab: { title, columns, records } }

// File -> [{ name, title, csv }]
// hasTitleRow: row 1 is a merged title and the real header is row 2 (your output workbook).
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

// ---- 1. new concerns: single tab ----
$('input').addEventListener('change', async e => {
  const file = e.target.files[0];
  $('compile').disabled = true;
  if (!file) return;

  const tabs = await readTabs(file, false);
  const n = Module.inputData(tabs[0].csv);   // first tab only
  status.textContent = `Loaded ${n} concerns from ${file.name}.`;
  $('compile').disabled = false;
});

// ---- 2. existing output: one tab per department, title in row 1 ----
$('existing').addEventListener('change', async e => {
  const file = e.target.files[0];
  Module.clearExisting();                    // always start clean
  if (!file) return;

  const tabs = await readTabs(file, true);
  let rows = 0;
  for (const { name, title, csv } of tabs) rows += Module.uploadExisting(name, title, csv);
  status.textContent = `Loaded ${tabs.length} tab(s), ${rows} rows from ${file.name}.`;
});

// ---- compile ----
$('compile').addEventListener('click', () => {
  compiled = JSON.parse(Module.compile());
  render(compiled);
  const tabs = Object.keys(compiled);
  status.textContent = `Compiled ${tabs.length} tab(s).`;
  $('download').disabled = tabs.length === 0;
});

// ---- download: title row (merged), header row, then data ----
$('download').addEventListener('click', () => {
  const wb = XLSX.utils.book_new();

  for (const [tab, t] of Object.entries(compiled)) {
    const ws = XLSX.utils.aoa_to_sheet([[t.title], t.columns]);
    XLSX.utils.sheet_add_json(ws, t.records, { origin: 'A3', header: t.columns, skipHeader: true });
    ws['!merges'] = [{ s: { r: 0, c: 0 }, e: { r: 0, c: Math.max(t.columns.length - 1, 0) } }];
    XLSX.utils.book_append_sheet(wb, ws, safeSheetName(tab));
  }

  XLSX.writeFile(wb, 'compiled.xlsx');
});

// Excel: max 31 chars, none of  \ / ? * [ ]  :
const safeSheetName = s => s.replace(/[\\/?*[\]:]/g, '_').slice(0, 31);

// ---- preview ----
function render(data) {
  const result = $('result');
  result.replaceChildren();

  for (const [tab, t] of Object.entries(data)) {
    const h = document.createElement('h3');
    h.textContent = `${t.title} (${t.records.length})`;
    result.appendChild(h);

    const table = document.createElement('table');
    const head = table.createTHead().insertRow();
    for (const c of t.columns) {
      const th = document.createElement('th');
      th.textContent = c;
      head.appendChild(th);
    }
    const body = table.createTBody();
    for (const rec of t.records) {
      const tr = body.insertRow();
      for (const c of t.columns) tr.insertCell().textContent = rec[c] ?? '';
    }
    result.appendChild(table);
  }
}
