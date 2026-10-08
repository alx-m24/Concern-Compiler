import createModule from './app.js';

const $ = id => document.getElementById(id);
const status = $('status');

const Module = await createModule();

const compiler = new Module.ConcernCompiler("Concern Compiler");

status.textContent = 'WASM ready.';

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

// 1. new concerns file uploaded
$('input').addEventListener('change', async e => {
    console.log("Here");
    const file = e.target.files[0];
    $('compile').disabled = true;
    if (!file) return;

    const tabs = await readTabs(file, false);
    const n = compiler.inputData(tabs[0].csv);   // first tab only
    status.textContent = `Loaded ${n} concerns from ${file.name}.`;
    $('compile').disabled = false;

    $('result').textContent = compiler.getInputConcerns();
});
