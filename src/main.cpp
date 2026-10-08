#include <string>
#include <emscripten/bind.h>

#include "D1.hpp"
#include "LLM.hpp"

class ConcernCompiler {
    private:
        std::string name{};
        const std::string* inputConcerns{};

        const std::string NULL_STR = "null";

    public:
        ConcernCompiler(std::string name) : name(name) {
            if (!test_llm()) {
                printf("C++: LLM Call Failed");
                return;
            }
            if (!test_D1()) {
                printf("C++: D1 Call Failed");
                return;
            }
        }

        int inputData(const std::string& csvText) {
            inputConcerns = &csvText;
            return 71224;
        }

    public:
        const std::string& getInputConcerns() {
            return inputConcerns == nullptr ? NULL_STR : *inputConcerns;
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
};

EMSCRIPTEN_BINDINGS(ConcernCompiler) {
    emscripten::class_<ConcernCompiler>("ConcernCompiler")
        .constructor<std::string>()
        .function("inputData", &ConcernCompiler::inputData)
        .function("getInputConcerns", &ConcernCompiler::getInputConcerns);
}
