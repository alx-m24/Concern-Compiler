#include <string>
#include <emscripten/bind.h>
#include <nlohmann/json.hpp>

#include "D1.hpp"
#include "LLM.hpp"

class ConcernCompiler {
    public:
        struct RawConcern {
            std::string timestamp{};
            std::string sentence{};
        };

    private:
        std::string name{};
        std::string inputConcerns{};

        const std::string NULL_STR = "null";

    private:
        std::string currentDate{};
        uint32_t totaltokensUsed{};

    public:
       ConcernCompiler(std::string name) : name(name) {}

        bool init() {
            if (!test_D1()) { printf("C++: D1 Call Failed\n"); return false; }
        
            APIResult dateResult = D1_Query("SELECT date('now') AS current_date;");
            auto dateJSON = nlohmann::json::parse(dateResult.response);
            currentDate = dateJSON["results"][0]["current_date"].get<std::string>();
        
            APIResult tokensResult = D1_Query("SELECT * FROM settings WHERE key = 'LLM_Tokens';");
            auto tokensJSON = nlohmann::json::parse(tokensResult.response);
            auto& rows = tokensJSON["results"];
            if (!rows.empty() && rows[0]["Date Entered"].get<std::string>() == currentDate)
                totaltokensUsed = std::stoul(rows[0]["value"].get<std::string>());
        
            uint32_t tokensUsed{};
            if (!test_llm(tokensUsed)) { printf("C++: LLM Call Failed\n"); return false; }
            totaltokensUsed += tokensUsed;
            return true;
        }
        int inputData(const std::string& csvText) {
            inputConcerns = csvText;
            return 71224;
        }

    public:
        const std::string& getInputConcerns() {
            return inputConcerns.empty() ? NULL_STR : inputConcerns;
        }

        const std::string& compile(float& out_ProgressPercentage) {
            // FOR EACH INPUT CONCERN
        
            // 1. LLM:
            //    Category: Facilities      NOTE: Category can be "Invalid"
            //    Keywords: ["water dispenser", "Block E", "not dispensing"]
            //    Rewritten: "The water dispenser in Block E is not dispensing water."
        
            // 2. Invalid?
            //    -> discard
        
            // 3. D1:
            //    Find concerns sharing ANY relevant keywords
            //
            //    "water dispenser"
            //    "Block E"
            //    "not dispensing"
            //
            //    -> candidate concerns
        
            // 4. LLM:
            //    Compare the new rewritten concern against candidate rewrites.
            //
            //    -> duplicate / not duplicate
        
            // 5. Duplicate?
            //    -> increment existing concern count
            //
            //    Otherwise:
            //    -> INSERT new concern
        }

        void save() {
            std::string sqlQuery =
                "UPDATE settings SET "
                "value = " + std::to_string(totaltokensUsed) +
                ", 'Date Entered' = '" + currentDate + "' "
                "WHERE key = 'LLM_Tokens';";

            APIResult writeTokensUsed =  D1_Query(sqlQuery);
        }
};

EMSCRIPTEN_BINDINGS(ConcernCompiler) {
    emscripten::class_<ConcernCompiler>("ConcernCompiler")
        .constructor<std::string>()
        .function("init", &ConcernCompiler::init, emscripten::async())
        .function("save", &ConcernCompiler::save, emscripten::async())
        .function("inputData", &ConcernCompiler::inputData)
        .function("getInputConcerns", &ConcernCompiler::getInputConcerns);
}
