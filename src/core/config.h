#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace config {

inline const std::string TOOL_NAME = "Sentinel-C";
inline const std::string VERSION = "v4.0";
inline const std::string CODENAME = "AEGIS";

namespace fs = std::filesystem;

inline fs::path detect_project_root() {
    std::error_code ec;
    fs::path current = fs::current_path(ec);
    if (ec) {
        return fs::path(".");
    }

    for (fs::path candidate = current; !candidate.empty(); candidate = candidate.parent_path()) {
        std::error_code check_ec;
        const bool has_cmake = fs::exists(candidate / "CMakeLists.txt", check_ec);
        const bool has_src = fs::exists(candidate / "src", check_ec);
        if (!check_ec && has_cmake && has_src) {
            return candidate;
        }

        if (candidate == candidate.root_path()) {
            break;
        }
    }

    return current;
}

inline fs::path resolve_project_root() {
    const char* env_root = std::getenv("SENTINEL_ROOT");
    if (env_root != nullptr && env_root[0] != '\0') {
        return fs::path(env_root);
    }
    return detect_project_root();
}

inline std::string normalize_path_string(const fs::path& path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.lexically_normal().generic_string();
}

inline const std::string PROJECT_ROOT = normalize_path_string(resolve_project_root());
inline const std::string ROOT_DIR = normalize_path_string(fs::path(PROJECT_ROOT) / "sentinel-c-logs");
inline const std::string DATA_DIR = normalize_path_string(fs::path(ROOT_DIR) / "data");
inline const std::string LOG_DIR = normalize_path_string(fs::path(ROOT_DIR) / "logs");
inline const std::string REPORT_DIR = normalize_path_string(fs::path(ROOT_DIR) / "reports");

inline const std::string REPORT_CLI_DIR = normalize_path_string(fs::path(REPORT_DIR) / "cli");
inline const std::string REPORT_HTML_DIR = normalize_path_string(fs::path(REPORT_DIR) / "html");
inline const std::string REPORT_JSON_DIR = normalize_path_string(fs::path(REPORT_DIR) / "json");
inline const std::string REPORT_CSV_DIR = normalize_path_string(fs::path(REPORT_DIR) / "csv");

inline const std::string BASELINE_DB = normalize_path_string(fs::path(DATA_DIR) / ".sentinel-baseline");
inline const std::string LOG_FILE = normalize_path_string(fs::path(LOG_DIR) / ".sentinel-logs");
inline const std::string IGNORE_FILE = normalize_path_string(fs::path(PROJECT_ROOT) / ".sentinelignore");

inline const bool COLOR_OUTPUT = true;

} // namespace config
