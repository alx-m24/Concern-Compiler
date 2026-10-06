#include <emscripten/bind.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using Rows = std::vector<std::vector<std::string>>;

static Rows parseCsv(const std::string& text) {
    Rows rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }  // escaped quote
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
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;  // CRLF
            if (field.empty() && row.empty()) continue;                         // skip blank lines
            row.push_back(std::move(field));
            field.clear();
            rows.push_back(std::move(row));
            row.clear();
        } else {
            field += c;
        }
    }

    if (!field.empty() || !row.empty()) {  // last line without trailing newline
        row.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

using ordered_json = nlohmann::ordered_json;

// JS passes the raw file text in, gets a JSON string back.
std::string processCsv(const std::string& text) {
    Rows rows = parseCsv(text);
    ordered_json out = ordered_json::array();
    if (rows.empty()) return out.dump();

    const auto& header = rows[0];
    for (size_t r = 1; r < rows.size(); ++r) {
        ordered_json obj = ordered_json::object();
        for (size_t c = 0; c < header.size(); ++c)
            obj[header[c]] = c < rows[r].size() ? rows[r][c] : "";   // pad short rows
        out.push_back(std::move(obj));
    }
    return out.dump();
}

EMSCRIPTEN_BINDINGS(csv_module) {
    emscripten::function("processCsv", &processCsv);
}
