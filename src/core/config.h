#pragma once
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace config {

inline const std::string TOOL_NAME = "Sentinel-C";
inline const std::string VERSION = "v4.5";
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

inline fs::path detect_executable_path() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(std::string(buffer, len));
    }
    return fs::path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return fs::path();
    }
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        return fs::path(buffer.c_str());
    }
    return fs::path();
#else
    char buffer[PATH_MAX] = {0};
    const ssize_t len = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        return fs::path(buffer);
    }
    return fs::path();
#endif
}

inline fs::path detect_binary_root() {
    std::error_code ec;
    const fs::path exe = detect_executable_path();
    if (!exe.empty()) {
        const fs::path parent = exe.parent_path();
        if (!parent.empty() && fs::exists(parent, ec) && !ec) {
            return parent;
        }
    }
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        return cwd;
    }
    return fs::path(".");
}

inline fs::path resolve_output_root() {
    const char* env_root = std::getenv("SENTINEL_ROOT");
    if (env_root != nullptr && env_root[0] != '\0') {
        return fs::path(env_root);
    }
    return detect_binary_root();
}

inline std::string normalize_path_string(const fs::path& path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.lexically_normal().generic_string();
}

inline std::tm local_time(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

inline std::string build_run_id() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) %
                    1000;
    const std::tm tm = local_time(t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << "_" << std::setw(3) << std::setfill('0') << ms.count();
    return out.str();
}

inline std::string PROJECT_ROOT = normalize_path_string(detect_project_root());
inline std::string OUTPUT_ROOT = normalize_path_string(resolve_output_root());
inline std::string RUN_ID = build_run_id();

inline std::string ROOT_DIR;
inline std::string DATA_DIR;
inline std::string LOG_DIR;
inline std::string REPORT_DIR;

inline std::string REPORT_CLI_DIR;
inline std::string REPORT_HTML_DIR;
inline std::string REPORT_JSON_DIR;
inline std::string REPORT_CSV_DIR;

inline std::string BASELINE_DB;
inline std::string BASELINE_SEAL_FILE;
inline std::string LOG_FILE;
inline std::string IGNORE_FILE;

inline void rebuild_paths() {
    ROOT_DIR = normalize_path_string(fs::path(OUTPUT_ROOT) / "sentinel-c-logs");
    DATA_DIR = normalize_path_string(fs::path(ROOT_DIR) / "data");
    LOG_DIR = normalize_path_string(fs::path(ROOT_DIR) / "logs");
    REPORT_DIR = normalize_path_string(fs::path(ROOT_DIR) / "reports");

    REPORT_CLI_DIR = normalize_path_string(fs::path(REPORT_DIR) / "cli");
    REPORT_HTML_DIR = normalize_path_string(fs::path(REPORT_DIR) / "html");
    REPORT_JSON_DIR = normalize_path_string(fs::path(REPORT_DIR) / "json");
    REPORT_CSV_DIR = normalize_path_string(fs::path(REPORT_DIR) / "csv");

    BASELINE_DB = normalize_path_string(fs::path(DATA_DIR) / ".sentinel-baseline");
    BASELINE_SEAL_FILE = normalize_path_string(fs::path(DATA_DIR) / ".sentinel-baseline.seal");
    LOG_FILE = normalize_path_string(fs::path(LOG_DIR) / ("sentinel-c_activity_log_" + RUN_ID + ".log"));
    IGNORE_FILE = normalize_path_string(fs::path(OUTPUT_ROOT) / ".sentinelignore");
}

inline bool set_output_root(const std::string& path, std::string* error = nullptr) {
    if (path.empty()) {
        if (error != nullptr) {
            *error = "output destination path is empty";
        }
        return false;
    }

    std::error_code ec;
    fs::path root(path);
    fs::create_directories(root, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "failed to create destination directory: " + ec.message();
        }
        return false;
    }

    OUTPUT_ROOT = normalize_path_string(root);
    rebuild_paths();
    return true;
}

inline const std::string& output_root() {
    return OUTPUT_ROOT;
}

struct RuntimeInit {
    RuntimeInit() {
        rebuild_paths();
    }
};

inline RuntimeInit RUNTIME_INIT;

inline const bool COLOR_OUTPUT = true;

} // namespace config
