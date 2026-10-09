#pragma once

#include <algorithm>
#include <vector>
#include <nlohmann/json.hpp>

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
