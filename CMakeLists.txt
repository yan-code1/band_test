cmake_minimum_required(VERSION 3.16)
project(nb VERSION 1.0.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Static CRT (no vcredist dependency) ───────────────────────
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── Compiler Flags ────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX)       # warnings-as-errors
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")  # Chinese comments
    add_compile_options(/Oy-)          # disable frame-pointer omission
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)  # suppress safe CRT warnings
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ── Dependencies: header-only libraries in third_party/ ──────
# CLI11 (v2.2.0) and nlohmann/json (v3.12.0)
# Download from GitHub if missing:
#   git clone --depth 1 --branch v2.2.0 https://github.com/CLIUtils/CLI11.git /tmp/cli11
#   git clone --depth 1 --branch v3.11.2 https://github.com/nlohmann/json.git /tmp/json
# Then copy include/CLI/ → third_party/CLI/ and single_include/ → third_party/

# ── Sources ───────────────────────────────────────────────────
set(NB_SOURCES
    src/main.cpp
    src/argument_parser.cpp
    src/server_handler.cpp
    src/client_handler.cpp
    src/control_protocol.cpp
    src/udp_socket.cpp
    src/pacer.cpp
    src/stats_collector.cpp
    src/reporter.cpp
    src/console_handler.cpp
)

set(NB_HEADERS
    src/argument_parser.hpp
    src/server_handler.hpp
    src/client_handler.hpp
    src/control_protocol.hpp
    src/udp_socket.hpp
    src/pacer.hpp
    src/stats_collector.hpp
    src/reporter.hpp
    src/console_handler.hpp
    src/protocol_header.hpp
    src/platform.hpp
)

# ── Executable ────────────────────────────────────────────────
add_executable(nb ${NB_SOURCES} ${NB_HEADERS})

# ── Include directories ───────────────────────────────────────
target_include_directories(nb PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

# ── Link ──────────────────────────────────────────────────────
target_link_libraries(nb PRIVATE
    ws2_32.lib
)

# ── Output ────────────────────────────────────────────────────
set_target_properties(nb PROPERTIES
    OUTPUT_NAME "nb"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── Install ───────────────────────────────────────────────────
install(TARGETS nb RUNTIME DESTINATION bin)
