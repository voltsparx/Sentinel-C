cmake_minimum_required(VERSION 3.10)
project(SentinelC VERSION 4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build only the modular implementation tree.
set(SENTINEL_SOURCES
    src/main.cpp
    src/cli.cpp
    src/commands/arg_parser.cpp
    src/commands/common.cpp
    src/commands/advisor.cpp
    src/commands/scan_ops.cpp
    src/commands/baseline_ops.cpp
    src/commands/maintenance_ops.cpp
    src/commands/prompt_console.cpp
    src/commands/dispatcher.cpp
    src/banner.cpp
    src/core/fsutil.cpp
    src/core/logger.cpp
    src/core/summary.cpp
    src/scanner/scanner.cpp
    src/scanner/baseline.cpp
    src/scanner/ignore.cpp
    src/scanner/hash.cpp
    src/reports/cli_report.cpp
    src/reports/advice.cpp
    src/reports/html_report.cpp
    src/reports/json_report.cpp
    src/reports/csv_report.cpp
)

add_executable(sentinel-c ${SENTINEL_SOURCES})
target_include_directories(sentinel-c PRIVATE src)
set_target_properties(sentinel-c PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(sentinel-c PRIVATE /W4 /permissive-)
else()
    target_compile_options(sentinel-c PRIVATE -Wall -Wextra -pedantic -Wshadow)
endif()

message(STATUS "Sentinel-C v${PROJECT_VERSION} build configured.")
