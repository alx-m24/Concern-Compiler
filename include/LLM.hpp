#pragma once

#include <emscripten.h>
#include <functional>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "D1.hpp"
#include "api.hpp"

extern "C" {
    EM_ASYNC_JS(char*, LLM_Call_JS, (const char* prompt), {
        const promptString = UTF8ToString(prompt);

        console.log("LLM prompt:", promptString);

        try {
            const response = await fetch(
                "https://tusc-concern-compiler.alexandremohamudally.workers.dev/api/llm",
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({
                        prompt: promptString
                    })
                }
            );

            const body = await response.text();

            console.log("LLM response:", body);

            let output;

            if (!response.ok) {
                output = JSON.stringify({
                    success: false,
                    status: response.status,
                    error: body
                });
            }
            else {
                let result;

                try {
                    result = JSON.parse(body);

                    const text =
                        result.choices[0].message.content;

                    const neurons =
                        result.usage?.neurons ?? 0;

                    console.log("LLM response:", text);
                    console.log("LLM neurons:", neurons);

                    output = JSON.stringify({
                        success: true,
                        status: response.status,
                        response: text,
                        neurons: neurons
                    });

                } catch (error) {
                    output = JSON.stringify({
                        success: false,
                        status: response.status,
                        error: "Invalid LLM response: " + String(error)
                    });
                }
            }

            const size = lengthBytesUTF8(output) + 1;
            const ptr = _malloc(size);

            stringToUTF8(output, ptr, size);

            return ptr;

        } catch (error) {

            const output = JSON.stringify({
                success: false,
                status: 0,
                error: String(error)
            });

            const size = lengthBytesUTF8(output) + 1;
            const ptr = _malloc(size);

            stringToUTF8(output, ptr, size);

            return ptr;
        }
    });
}

    inline APIResult LLM_Call(
        const std::string& prompt,
        float* neurons = nullptr
    ) {
        char* raw = LLM_Call_JS(prompt.c_str());

        nlohmann::json result = nlohmann::json::parse(raw);

        free(raw);

        if (!result["success"].get<bool>()) {
            int status = result["status"].get<int>();

            std::string error = result["error"].get<std::string>();

            printf(
                "LLM ERROR [%d]: %s\n",
                status,
                error.c_str()
            );

            return APIResult(error, status);
        }

        if (neurons) {
            *neurons = result["neurons"].get<float>();
        }

        return APIResult(
            result["response"].get<std::string>()
        );
    }

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    bool test_llm(float& neurons) {
        printf("C++: before LLM call\n");

        APIResult result = LLM_Call("hello", &neurons);

        if (!result.success) {
            printf(
                "C++: LLM Error [%d]: %s\n",
                result.status,
                result.error.c_str()
            );

            return false;
        }

        printf(
            "C++: LLM returned: %s\n",
            result.response.c_str()
        );

        printf(
            "C++: neurons: %f\n",
            neurons
        );

        return true;
    }
}

class LLM {
    private:
        std::string currentDate{};
        float totalneuronsUsed{};

        const float MAX_NEURONS = 10'000;

    public:
        LLM() = default;

        bool init() {
            APIResult dateResult = D1_Query("SELECT date('now') AS current_date;");
            auto dateJSON = nlohmann::json::parse(dateResult.response);
            currentDate = dateJSON["results"][0]["current_date"].get<std::string>();
        
            APIResult neuronsResult = D1_Query("SELECT * FROM settings WHERE key = 'LLM_Neurons';");
            auto neuronsJSON = nlohmann::json::parse(neuronsResult.response);
            auto& rows = neuronsJSON["results"];
            if (!rows.empty() && rows[0]["Date Entered"].get<std::string>() == currentDate) {
                totalneuronsUsed = std::stof(rows[0]["value"].get<std::string>());
                printf("C++ totalneuronsUsed: %f\n", totalneuronsUsed);
            }
        
            if (getNeuronPercentage() >= 100.0f) { printf("C++: neuron cap reached\n"); return false; }

            float neuronsUsed{};
            if (!test_llm(neuronsUsed)) { printf("C++: LLM Call Failed\n"); return false; }

            totalneuronsUsed += neuronsUsed;

            save();

            return true;
        }

        float getNeuronPercentage() const {
            return totalneuronsUsed / MAX_NEURONS * 100.0f;
        }

        APIResult Call(const std::string& prompt) {
            float neurons{};
            APIResult result = LLM_Call(prompt, &neurons);
            totalneuronsUsed += neurons;

            save();
            
            return result;
        }

    private:
        void save() {
            std::string sqlQuery =
                "UPDATE settings SET "
                "value = " + std::to_string(totalneuronsUsed) +
                ", 'Date Entered' = '" + currentDate + "' "
                "WHERE key = 'LLM_Neurons';";

            APIResult writeneuronsUsed =  D1_Query(sqlQuery);
        }
};
