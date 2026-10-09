cmake_minimum_required(VERSION 3.24)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # writes build/compile_commands.json

project(Concern-Compiler
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)

# ---- nlohmann/json: header-only, no tests/install ----
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# ---- Sources ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- WASM (configure with emcmake) ----
if(EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME app                                   # emits app.js + app.wasm
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/web
    )

    # nlohmann/json throws; Emscripten disables C++ exceptions by default
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -fexceptions
        -Werror=return-type
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -fexceptions
        -lembind
        -sASYNCIFY
        -sEXPORT_ES6=1
        -sMODULARIZE=1
        -sEXPORT_NAME=createModule
        -sALLOW_MEMORY_GROWTH=1
        --no-entry
    )
endif()
