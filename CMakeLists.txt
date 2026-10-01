cmake_minimum_required(VERSION 3.20)

project(Concern-Compiler
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)

# --------------------------------------------------
# Dependencies
# --------------------------------------------------

find_package(CURL REQUIRED)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(nlohmann_json)

find_package(nlohmann_json REQUIRED)

# --------------------------------------------------
# Sources
# --------------------------------------------------

file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

# --------------------------------------------------
# Executable
# --------------------------------------------------

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

# --------------------------------------------------
# Include directories
# --------------------------------------------------

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# --------------------------------------------------
# Libraries
# --------------------------------------------------

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# --------------------------------------------------
# Compiler warnings
# --------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            /W4
)
endif()
