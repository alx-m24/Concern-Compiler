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
    const records = JSON.parse(Module.processCsv(text));
    render(records);
    status.textContent = `${file.name}: ${records.length} records`;
});

function render(records) {
  result.replaceChildren();
  if (records.length === 0) return;

  const cols = Object.keys(records[0]);
  const table = document.createElement('table');
  const head = table.createTHead().insertRow();
  for (const c of cols) {
    const th = document.createElement('th');
    th.textContent = c;
    head.appendChild(th);
  }
  const body = table.createTBody();
  for (const rec of records) {
    const tr = body.insertRow();
    for (const c of cols) tr.insertCell().textContent = rec[c];
  }
  result.appendChild(table);
}
