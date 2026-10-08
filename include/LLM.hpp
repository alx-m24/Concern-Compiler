#pragma once

#include <emscripten.h>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>

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

                    const tokens =
                        result.usage?.total_tokens ?? 0;

                    console.log("LLM response:", text);
                    console.log("LLM tokens:", tokens);

                    output = JSON.stringify({
                        success: true,
                        status: response.status,
                        response: text,
                        tokens: tokens
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

    inline APIResult LLM_Call(
        const std::string& prompt,
        int* tokens = nullptr
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

        if (tokens) {
            *tokens = result["tokens"].get<int>();
        }

        return APIResult(
            result["response"].get<std::string>()
        );
    }

    EMSCRIPTEN_KEEPALIVE
    bool test_llm() {
        printf("C++: before LLM call\n");

        int tokens = 0;

        APIResult result = LLM_Call("hello", &tokens);

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
            "C++: tokens: %d\n",
            tokens
        );

        return true;
    }

}
