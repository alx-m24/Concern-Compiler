#pragma once

#include <emscripten.h>
#include <string>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "api.hpp"

extern "C" {

    EM_ASYNC_JS(char*, D1_Query_JS, (const char* query), {
        const queryString = UTF8ToString(query);
    
        console.log("D1 query:", queryString);
    
        try {
            const response = await fetch(
                "https://tusc-concern-compiler.alexandremohamudally.workers.dev/api/d1",
                {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/json"
                    },
                    body: JSON.stringify({
                        sql: queryString
                    })
                }
            );
    
            const body = await response.text();
    
            console.log("D1 response:", body);
    
            let output;
    
            if (!response.ok) {
                output = JSON.stringify({
                    success: false,
                    status: response.status,
                    error: body
                });
            }
            else {
                output = JSON.stringify({
                    success: true,
                    status: response.status,
                    response: body
                });
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

    inline APIResult D1_Query(const std::string& query) {
        char* raw = D1_Query_JS(query.c_str());
    
        nlohmann::json result = nlohmann::json::parse(raw);
    
        free(raw);
    
        if (!result["success"].get<bool>()) {
            int status = result["status"].get<int>();
    
            std::string error = result["error"].get<std::string>();
    
            printf(
                "D1 ERROR [%d]: %s\n",
                status,
                error.c_str()
            );
    
            return APIResult(error, status);
        }
    
        return APIResult(result["response"].get<std::string>());
    }

    EMSCRIPTEN_KEEPALIVE
    bool test_D1() {
        printf("C++: before D1 call\n");

        APIResult result = D1_Query(
            "SELECT name FROM sqlite_schema "
            "WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%';"
        );

        if (!result.success) {
            printf("C++: D1 Error: %s\n", result.error.c_str()); 
            return false;
        }

        printf("C++: D1 returned: %s\n", result.response.c_str());

        return true;
    }
}
