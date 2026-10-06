import createModule from './app.js';

const status = document.getElementById('status');
const result = document.getElementById('result');
const input  = document.getElementById('file');

const Module = await createModule();
status.textContent = 'WASM ready. Choose a CSV file.';

input.addEventListener('change', async () => {
  const file = input.files[0];
  if (!file) return;

  if (!file.name.toLowerCase().endsWith('.csv')) {
    status.textContent = 'Please select a .csv file.';
    return;
  }

  const text = await file.text();

  // C++ call: string in, JSON string out
  const rows = JSON.parse(Module.processCsv(text));

  render(rows);
  status.textContent = `${file.name}: ${rows.length} rows (including header)`;
});

function render(rows) {
  result.replaceChildren();
  if (rows.length === 0) return;

  const table = document.createElement('table');
  const thead = table.createTHead().insertRow();
  for (const h of rows[0]) {
    const th = document.createElement('th');
    th.textContent = h;           // textContent, not innerHTML, so CSV content can't inject HTML
    thead.appendChild(th);
  }

  const tbody = table.createTBody();
  for (const r of rows.slice(1)) {
    const tr = tbody.insertRow();
    for (const cell of r) tr.insertCell().textContent = cell;
  }
  result.appendChild(table);
}
