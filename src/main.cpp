#include <emscripten/bind.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>

using ordered_json = nlohmann::ordered_json;
using Rows = std::vector<std::vector<std::string>>;

// ---------- helpers ----------

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(ws) - a + 1);
}

static Rows parseCsv(const std::string& text) {
    Rows rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
                else quoted = false;
            } else {
                field += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            row.push_back(std::move(field));
            field.clear();
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            if (field.empty() && row.empty()) continue;
            row.push_back(std::move(field));
            field.clear();
            rows.push_back(std::move(row));
            row.clear();
        } else {
            field += c;
        }
    }

    if (!field.empty() || !row.empty()) {
        row.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

// CSV text -> { "columns": [...], "records": [{...}, ...] }
// Headers are trimmed, empty headers are ignored, and duplicate headers are merged:
// the first column's position wins, a later value is taken only if the earlier is empty.
static ordered_json csvToTable(const std::string& text) {
    Rows rows = parseCsv(text);

    std::vector<std::string> header, columns;
    if (!rows.empty())
        for (auto& h : rows[0]) header.push_back(trim(h));
    for (auto& h : header)
        if (!h.empty() && std::find(columns.begin(), columns.end(), h) == columns.end())
            columns.push_back(h);

    ordered_json records = ordered_json::array();
    static const std::string empty;

    for (size_t r = 1; r < rows.size(); ++r) {
        ordered_json obj = ordered_json::object();
        for (size_t c = 0; c < header.size(); ++c) {
            if (header[c].empty()) continue;
            const std::string& val = c < rows[r].size() ? rows[r][c] : empty;
            auto it = obj.find(header[c]);
            if (it == obj.end())
                obj[header[c]] = val;
            else if (it->get<std::string>().empty())
                *it = val;
        }
        records.push_back(std::move(obj));
    }

    ordered_json t = ordered_json::object();
    t["columns"] = columns;
    t["records"] = std::move(records);
    return t;
}

// ---------- state ----------

static ordered_json g_input    = ordered_json::object();  // { columns, records } (single table)
static ordered_json g_existing = ordered_json::object();  // { "Tab": { title, columns, records } }

// ---------- exposed to JS ----------

int inputData(const std::string& csvText) {
    g_input = csvToTable(csvText);
    return (int)g_input["records"].size();
}

// called once per tab of the existing output workbook
int uploadExisting(const std::string& tab, const std::string& title, const std::string& csvText) {
    ordered_json t = csvToTable(csvText);
    ordered_json entry = ordered_json::object();
    entry["title"]   = title;
    entry["columns"] = t["columns"];
    entry["records"] = t["records"];
    g_existing[tab] = std::move(entry);
    return (int)g_existing[tab]["records"].size();
}

void clearExisting() { g_existing = ordered_json::object(); }

// returns JSON: { "Tab": { title, columns, records } }
std::string compile() {
    ordered_json out = g_existing;   // start from what's already compiled

    // TODO: for each record in g_input["records"]:
    //   - pick the department tab
    //   - check the reference cells in out[tab]["records"] to skip duplicates
    //   - out[tab]["records"].push_back(...) if new
   
    const std::string DEPARTMENT_HEADER_KEY = "What area would you like to provide feedback/concern on?";
    const std::unordered_map<std::string, std::string> TAB_TO_DEPT_INPUT = {
        { "Academics", "Academics - [TES, Course Content etc]" },
        { "Facilities", "Campus Facilities - [Study Areas, Restrooms, Parking Space, Transportation Services etc]" },
    };

    for (const auto& input : g_input["records"]) {
        std::string dep = input.value(DEPARTMENT_HEADER_KEY, "");
    }

    if (out.empty() && !g_input.empty()) {   // placeholder passthrough
        ordered_json t = ordered_json::object();
        t["title"]   = "Unsorted";
        t["columns"] = g_input["columns"];
        t["records"] = g_input["records"];
        out["Unsorted"] = std::move(t);
    }
    return out.dump();
}

EMSCRIPTEN_BINDINGS(concern_module) {
    emscripten::function("inputData",      &inputData);
    emscripten::function("uploadExisting", &uploadExisting);
    emscripten::function("clearExisting",  &clearExisting);
    emscripten::function("compile",        &compile);
}
