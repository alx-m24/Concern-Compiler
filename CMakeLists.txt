cmake_minimum_required(VERSION 3.24)

project(Concern-Compiler
    VERSION 1.0
    LANGUAGES C CXX            # curl is C, so C must be enabled
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)

include(FetchContent)

# ---- curl: static, library only, no optional deps ----
set(BUILD_CURL_EXE      OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY           ON  CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL     OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2    OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH     OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2         OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2         OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI         OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD           OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP   ON  CACHE BOOL "" FORCE)

# TLS backend: use the OS-native one where it exists
if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)   # built into Windows
    set(CURL_USE_OPENSSL  OFF CACHE BOOL "" FORCE)
endif()
# macOS: curl picks the native TLS automatically.
# Linux: needs OpenSSL dev headers (see note below).

FetchContent_Declare(
    curl
    URL https://github.com/curl/curl/releases/download/curl-8_11_1/curl-8.11.1.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# ---- nlohmann/json: header-only, no tests/install ----
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(curl nlohmann_json)

# ---- Sources ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

add_executable(${PROJECT_NAME} ${SOURCES})

if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE
        -static            # static libstdc++, libgcc, and winpthread
    )
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
