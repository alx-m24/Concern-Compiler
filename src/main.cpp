#include <string>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <vector>
#include <format>

#include "LLM.hpp"
#include "D1.hpp"
#include "csv.hpp"
#include "prompts.hpp"

EM_JS(void, reportCompilationProgress, (float percentage), {
    window.compilationProgress = percentage;
});

namespace fs = std::filesystem;

class ConcernCompiler {
    public:
        struct RawConcern {
            std::string timestamp{};
            std::string sentence{};
        };

    private:
        std::string name{};
        std::string inputFileName{};
        std::string inputConcernsStr{};
        nlohmann::ordered_json inputConcerns{};
        uint32_t compiledConcernsNum{};

    private:
        LLM llm{};

    public:
       ConcernCompiler(std::string name) : name(name) {}

        bool init() {
            if (!test_D1()) { printf("C++: D1 Call Failed\n"); return false; }
        

            bool success = llm.init();
            printf("C++ Init success: %s\n", success ? "true" : "false");

            return success;
        }

        uint32_t inputData(const std::string& csvText, const std::string& fileName) {
            inputConcernsStr = csvText;
            inputConcerns = csvToTable(csvText);
            inputFileName = fs::path(fileName).stem();
            return static_cast<uint32_t>(inputConcerns.at("records").size());
        }

        float getNeuronPercentage() const {
            return llm.getNeuronPercentage();
        }

    private:
        struct Department {
            std::string name;    // exactly as the model must answer (no trailing space)
            std::string scope;   // one-line description shown to the model
        };
        
        // inside the class
        inline static const std::vector<Department> DEPARTMENTS = {
            {"Academics",                    "teaching, lecturers, course content, lecture slides, exams, assessments, group assignments, TES"},
            {"Facilities",                   "buildings and grounds: lifts, restrooms, seating, study areas, labs and equipment, plug points, air-conditioning, parking, buggy and shuttle bus, water dispensers"},
            {"EHS",                          "environment, health and safety: hygiene hazards, pests, mould, smoking and vaping, injuries, tripping hazards, unsafe electrical faults, clinic"},
            {"ICT",                          "Wi-Fi, Taylor's app, myTIMeS, student portal, campus software, computers"},
            {"International Office",         "visas, immigration, international student support"},
            {"Fees and Finance",             "tuition fees, payments, refunds, scholarships"},
            {"Campus Security",              "gates, access control, outsiders on campus, theft, lockers, security guards, threats"},
            {"Timetabling",                  "class scheduling, clashes, class times, gaps between classes, OMR"},
            {"Student Development",          "clubs and societies, Student Life Center, welfare and wellbeing, internships and careers"},
            {"(SyopzMall) Commercial Area",  "Syopz Mall, food outlets, food prices, convenience store, retail"},
        };
        
        static std::string departmentList() {
            std::string out;
            for (const auto& d : DEPARTMENTS)
                out += d.name + " - " + d.scope + "\n";
            return out;
        }

        static std::string norm(const std::string& s) {
            std::string o; for (unsigned char c : s) if (std::isalnum(c)) o += (char)std::tolower(c); return o;
        }
        static const Department* findDepartment(const std::string& category) {
            const std::string c = norm(category);
            for (const auto& d : DEPARTMENTS) if (c.starts_with(norm(d.name))) return &d;
            if (c.contains("syopz") || c.contains("commercial")) return &DEPARTMENTS.back();
            printf("C++: unknown category '%s'\n", category.c_str());   // confirms the cause
            return nullptr;
        }

        static nlohmann::json parseModelJson(const std::string& s) {
            size_t a = s.find('{'), b = s.rfind('}');
            if (a == std::string::npos || b == std::string::npos || b < a)
                return nlohmann::json(nlohmann::json::value_t::discarded);
            return nlohmann::json::parse(s.substr(a, b - a + 1), nullptr, false);
        }

        static nlohmann::json parseModelArray(const std::string& s) {
            size_t a = s.find('['), b = s.rfind(']');
            if (a != std::string::npos && b != std::string::npos && b > a) {
                auto v = nlohmann::json::parse(s.substr(a, b - a + 1), nullptr, false);
                if (v.is_array()) return v;
            }
            auto o = parseModelJson(s);                       // model returned a bare object
            if (o.is_object()) return nlohmann::json::array({o});
            return nlohmann::json(nlohmann::json::value_t::discarded);
        }

        struct CompileStats {
            uint32_t InvalidNum{};
            uint32_t FailedNum{};
            uint32_t InputDuplicateNum{};
            uint32_t MeaningDuplicateNum{};
        };

    public:
        std::string getInputConcerns() {
            return inputConcerns.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }

        bool clearData() {
            return resetDB().success;
        }

        std::string compile() {
            CompileStats stats{};
            const std::string departments = departmentList();            
            compiledConcernsNum = {};

            APIResult existingKeywordresult = D1_Query(
                "SELECT keyword FROM keywords;"
            );
            if (!existingKeywordresult.success) {
                printf("C++ failed to get existing keywords\n");
                return "[]";
            }
            std::vector<std::string> existingKeywords;
            auto parsed = nlohmann::json::parse(existingKeywordresult.response, nullptr, false);
            if (parsed.is_discarded() || !parsed.contains("results") || !parsed["results"].is_array()) {
                printf("C++ bad keywords response\n");
                return "[]";
            }
            for (const auto& row : parsed["results"]) {
                if (row.contains("keyword") && row["keyword"].is_string()) {
                    existingKeywords.push_back(row["keyword"].get<std::string>());
                }
            }

            auto addKeywords = [&existingKeywords](const std::vector<std::string>& incoming) -> bool {
                bool ok = true;
                for (const auto& k : incoming) {
                    if (std::find(existingKeywords.begin(), existingKeywords.end(), k) != existingKeywords.end()) continue;
                    if (D1_Query("INSERT OR IGNORE INTO keywords (keyword) VALUES (" + sqlEscape(k) + ");").success)
                        existingKeywords.push_back(k);
                    else ok = false;
                }
                return ok;
            };
            
            auto existingKeywordList = [&existingKeywords]() {
                if (existingKeywords.empty()) return std::string("(none yet)");
                std::string out;
                for (const auto& k : existingKeywords) {
                    if (!out.empty()) out += ", ";
                    out += k;
                }
                return out;
            };

            const auto& records = inputConcerns["records"];
            for (size_t rowIndex = 0; rowIndex < records.size(); ++rowIndex) {
                if (llm.getNeuronPercentage() >= 100.0f) { printf("C++: neuron cap reached\n"); break; }

                const auto& concern = records[rowIndex];
                const std::string refId = inputFileName + "|" + std::to_string(rowIndex + 2); // +2 to account for headers in spreadsheet

                const float progress = static_cast<float>(++compiledConcernsNum) / static_cast<float>(inputConcerns["records"].size()) * 100.0f;

                reportCompilationProgress(progress);

                const std::string email = concern.value("Email address", "");
                const std::string ts    = concern.value("Timestamp", "");

                // Early out if this exact input has already been compiled
                APIResult r = D1_Query(std::format(
                    "SELECT 1 FROM submissions "
                    "WHERE email = {} AND timestamp = {} "
                    "LIMIT 1;",
                    sqlEscape(email),
                    sqlEscape(ts)
                ));
                
                if (!r.success) {
                    ++stats.FailedNum;
                    continue;
                }

                auto result = nlohmann::json::parse(r.response, nullptr, false);
                if (result.is_discarded() || !result.contains("results") || !result["results"].is_array()) { ++stats.FailedNum; continue; }
                if (!result["results"].empty()) { ++stats.InputDuplicateNum; continue; }

                r = D1_Query(std::format(
                    "INSERT INTO submissions (email, timestamp) VALUES ({0}, {1});",
                    sqlEscape(email), sqlEscape(ts)
                ));

                if (!r.success) {
                    ++stats.FailedNum;
                    printf("C++: row %s failed\n", refId.c_str());
                    continue;
                }

                // 1. LLM:
                //    Category: Facilities      NOTE: Category can be "Invalid"
                //    Keywords: ["water dispenser", "Block E", "not dispensing"]
                //    Rewritten: "The water dispenser in Block E is not dispensing water."
                std::string prompt = std::format(
                    CLASSIFY_PROMPT_TEMPLATE,
                    departments,
                    existingKeywordList(),
                    concern.value("What area would you like to provide feedback/concern on?", ""),
                    concern.value("Feedback or Concern Area", ""),
                    concern.value("What is your concern(s)?", ""),
                    concern.value("If you have any specific suggestions or preferred solutions, please let us know here.", "")
                );

                r = llm.Call(prompt);
                if (!r.success) { ++stats.FailedNum; continue; }

                auto items = parseModelArray(r.response);
                if (items.is_discarded() || items.empty()) { ++stats.FailedNum; continue; }
                for (const auto& j : items) {
                    if (llm.getNeuronPercentage() >= 100.0f) { printf("C++: neuron cap reached\n"); break; }

                    // 2. Invalid?
                    //    -> discard
                    if (!j.is_object() || !j.contains("category") || !j["category"].is_string()) { ++stats.FailedNum; continue; }

                    std::string category = trim(j["category"].get<std::string>());
                    if (category == "Invalid") { 
                        ++stats.InvalidNum;
                        continue;
                    }
                    
                    if (!j.contains("rewritten") || !j["rewritten"].is_string() ||
                        !j.contains("keywords")  || !j["keywords"].is_array()) { ++stats.FailedNum; continue; }
                    
                    const Department* dept = findDepartment(category);
                    if (!dept) { ++stats.FailedNum; continue; }
                    
                    std::vector<std::string> keywords;
                    for (const auto& k : j["keywords"]) {
                        if (!k.is_string()) continue;

                        std::string s = trim(k.get<std::string>());
                        std::transform(s.begin(), s.end(), s.begin(),
                                       [](unsigned char c) { return std::tolower(c); });

                        if (!s.empty() && std::find(keywords.begin(), keywords.end(), s) == keywords.end()) {
                            keywords.push_back(std::move(s));
                        }
                    }
                    if (!addKeywords(keywords)) {
                        printf("C++: Failed to add new keywords to DB\n");
                    }
                    auto rewritten = j.value("rewritten", "");
                    if (trim(rewritten).empty()) { ++stats.FailedNum; continue; }

                    // 3. D1:
                    //    Find concerns sharing ANY relevant keywords
                    struct Candidate { int64_t id; std::string rewritten; };
                    std::vector<Candidate> candidates;
                    
                    if (!keywords.empty()) {
                        std::string kwSql;
                        for (const auto& k : keywords) {
                            if (!kwSql.empty()) kwSql += ", ";
                            kwSql += sqlEscape(k);                      // sqlEscape already adds the quotes
                        }
                    
                        r = D1_Query(std::format(
                            "SELECT id, rewritten FROM ("
                                "SELECT c.rowid AS id, c.rewritten, "
                                       "(SELECT COUNT(*) FROM json_each(c.keywords) e WHERE e.value IN ({1})) AS shared "
                                "FROM concerns c WHERE c.category = {0}"
                            ") WHERE shared >= 1 ORDER BY shared DESC, id DESC LIMIT 10;",
                            sqlEscape(dept->name), kwSql
                        ));
                        if (!r.success) { ++stats.FailedNum; continue; }
                    
                        auto cand = nlohmann::json::parse(r.response, nullptr, false);
                        if (cand.is_discarded() || !cand.contains("results") || !cand["results"].is_array()) {
                            ++stats.FailedNum; continue;
                        }
                        for (const auto& row : cand["results"]) {
                            if (row.contains("id") && row["id"].is_number_integer() &&
                                row.contains("rewritten") && row["rewritten"].is_string())
                                candidates.push_back({row["id"].get<int64_t>(), row["rewritten"].get<std::string>()});
                        }
                    }
                    
                    // 4. LLM: compare the new rewrite against the candidates
                    int64_t duplicateOfId = -1;                         // -1 = not a duplicate
                    
                    if (!candidates.empty()) {
                        auto strip = [](std::string s) {                // prevent tag breakout
                            std::erase_if(s, [](char c) { return c == '<' || c == '>'; });
                            return s;
                        };
                    
                        std::string list;
                        for (size_t i = 0; i < candidates.size(); ++i)
                            list += std::to_string(i + 1) + ". " + strip(candidates[i].rewritten) + "\n";
                    
                        APIResult cmp = llm.Call(std::format(
                            DUPLICATE_DETECTION_PROMPT_TEMPLATE, strip(rewritten), list));
                        if (!cmp.success) { ++stats.FailedNum; continue; }   // don't insert: it might be a duplicate
                    
                        nlohmann::json d = parseModelJson(cmp.response);
                        if (d.is_discarded() || !d.is_object() ||
                            !d.contains("exists") || !d["exists"].is_boolean()) {
                            ++stats.FailedNum; continue;
                        }
                    
                        if (d["exists"].get<bool>()) {
                            if (!d.contains("index") || !d["index"].is_number_integer()) {
                                ++stats.FailedNum; continue;
                            }
                            const int64_t idx = d["index"].get<int64_t>();
                            if (idx < 1 || idx > static_cast<int64_t>(candidates.size())) {
                                ++stats.FailedNum; continue;
                            }
                            duplicateOfId = candidates[idx - 1].id;     // map back to the DB row; the model never sees ids
                        }
                    }
        
                    if (duplicateOfId == -1) {
                        r = D1_Query(std::format(
                            "INSERT INTO concerns (email, timestamp, category, keywords, rewritten, count, department, \"references\") "
                            "VALUES ({0}, {1}, {2}, {3}, {4}, 1, {5}, {6}); ",
                            sqlEscape(email), sqlEscape(ts), sqlEscape(dept->name),
                            sqlEscape(nlohmann::json(keywords).dump()),
                            sqlEscape(rewritten),
                            sqlEscape(dept->name),
                            sqlEscape(nlohmann::json::array({refId}).dump())
                        ));
                        if (!r.success) { ++stats.FailedNum; continue; }
                    } else {
                        r = D1_Query(std::format(
                            "UPDATE concerns "
                            "SET count = count + 1, "
                            "    \"references\" = json_insert(\"references\", '$[#]', {1}) "
                            "WHERE rowid = {0} "
                            "AND NOT EXISTS (SELECT 1 FROM json_each(concerns.\"references\") WHERE value = {1});",
                            duplicateOfId, sqlEscape(refId)));
                        if (!r.success) { ++stats.FailedNum; continue; }
                        ++stats.MeaningDuplicateNum;
                    }
                }
            }

            printf("C++: Invalid inputs num: %u\n", stats.InvalidNum);
            printf("C++: Failed inputs num: %u\n", stats.FailedNum);
            printf("C++: Input duplicate num: %u\n", stats.InputDuplicateNum);
            printf("C++: Meaning duplicate num: %u\n", stats.MeaningDuplicateNum);

            return ImportExisting();
        }

        std::string ImportExisting() {
            APIResult all = D1_Query("SELECT rowid AS id, timestamp, category, keywords, rewritten, count, \"references\" FROM concerns;");
            auto parsedAll = nlohmann::json::parse(all.response, nullptr, false);
            if (!all.success || parsedAll.is_discarded() || !parsedAll.contains("results")) return "[]";
            return parsedAll["results"].dump();
        }
};

EMSCRIPTEN_BINDINGS(ConcernCompiler) {
    emscripten::class_<ConcernCompiler>("ConcernCompiler")
        .constructor<std::string>()
        .function("init", &ConcernCompiler::init, emscripten::async())
        .function("compile", &ConcernCompiler::compile, emscripten::async())
        .function("clearData", &ConcernCompiler::clearData, emscripten::async())
        .function("ImportExisting", &ConcernCompiler::ImportExisting, emscripten::async())
        .function("inputData", &ConcernCompiler::inputData)
        .function("getInputConcerns", &ConcernCompiler::getInputConcerns)
        .function("getNeuronPercentage", &ConcernCompiler::getNeuronPercentage);
}
