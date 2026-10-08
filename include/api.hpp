#pragma once

#define API_BASE "https://tusc-concern-compiler.alexandremohamudally.workers.dev/api"

#include <string>

struct APIResult {
    bool success;
    int status;
    std::string response;
    std::string error;

    APIResult(std::string response) : success(true), status(200), response(response), error("") {}
    APIResult(std::string error, int status) : success(false), status(status), response(""), error(error) {}
};
