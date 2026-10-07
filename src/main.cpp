#include <string>
#include <emscripten/bind.h>

class ConcernCompiler {
    private:
        std::string name{};
        const std::string* inputConcerns{};

        const std::string NULL_STR = "null";

    public:
        ConcernCompiler(std::string name) : name(name) {}

        int inputData(const std::string& csvText) {
            inputConcerns = &csvText;
            return 71224;
        }

    public:
        const std::string& getInputConcerns() {
            return inputConcerns == nullptr ? NULL_STR : *inputConcerns;
        }
};

EMSCRIPTEN_BINDINGS(ConcernCompiler) {
    emscripten::class_<ConcernCompiler>("ConcernCompiler")
        .constructor<std::string>()
        .function("inputData", &ConcernCompiler::inputData)
        .function("getInputConcerns", &ConcernCompiler::getInputConcerns);
}
