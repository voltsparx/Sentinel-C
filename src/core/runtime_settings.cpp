#include "runtime_settings.h"
#include "config.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string trim_copy(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

const char* getenv_or_null(const char* key) {
    const char* value = std::getenv(key);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

fs::path detect_config_home() {
    if (const char* custom_home = getenv_or_null("SENTINEL_CONFIG_HOME");
        custom_home != nullptr) {
        return fs::path(custom_home);
    }

#ifdef _WIN32
    if (const char* app_data = getenv_or_null("APPDATA"); app_data != nullptr) {
        return fs::path(app_data);
    }
    if (const char* user_profile = getenv_or_null("USERPROFILE");
        user_profile != nullptr) {
        return fs::path(user_profile) / "AppData" / "Roaming";
    }
#else
    if (const char* xdg = getenv_or_null("XDG_CONFIG_HOME"); xdg != nullptr) {
        return fs::path(xdg);
    }
    if (const char* home = getenv_or_null("HOME"); home != nullptr) {
        return fs::path(home) / ".config";
    }
#endif

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (ec) {
        return fs::path(".");
    }
    return cwd;
}

fs::path settings_path() {
    return detect_config_home() / "sentinel-c" / "settings.ini";
}

} // namespace

namespace core {

std::string settings_file_path() {
    return config::normalize_path_string(settings_path());
}

std::optional<std::string> load_saved_output_root(std::string* error) {
    if (error != nullptr) {
        error->clear();
    }

    const fs::path file_path = settings_path();
    std::error_code ec;
    if (!fs::exists(file_path, ec)) {
        return std::nullopt;
    }
    if (ec) {
        if (error != nullptr) {
            *error = "failed to read settings file: " + ec.message();
        }
        return std::nullopt;
    }

    std::ifstream in(file_path);
    if (!in.is_open()) {
        if (error != nullptr) {
            *error = "failed to open settings file: " +
                     config::normalize_path_string(file_path);
        }
        return std::nullopt;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(line.substr(0, eq));
        const std::string value = trim_copy(line.substr(eq + 1));
        if (key == "output_root" && !value.empty()) {
            return value;
        }
    }

    return std::nullopt;
}

bool save_output_root(const std::string& output_root, std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    if (output_root.empty()) {
        if (error != nullptr) {
            *error = "output root cannot be empty";
        }
        return false;
    }

    const fs::path file_path = settings_path();
    const fs::path parent = file_path.parent_path();

    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "failed to create settings directory: " + ec.message();
        }
        return false;
    }

    const fs::path temp_file = file_path.string() + ".tmp";
    std::ofstream out(temp_file, std::ios::trunc);
    if (!out.is_open()) {
        if (error != nullptr) {
            *error = "failed to open temp settings file for write: " +
                     config::normalize_path_string(temp_file);
        }
        return false;
    }

    out << "# Sentinel-C runtime settings\n";
    out << "output_root=" << output_root << "\n";
    out.close();
    if (!out) {
        if (error != nullptr) {
            *error = "failed to flush settings file: " +
                     config::normalize_path_string(temp_file);
        }
        return false;
    }

    fs::rename(temp_file, file_path, ec);
    if (ec) {
        ec.clear();
        fs::remove(file_path, ec);
        ec.clear();
        fs::rename(temp_file, file_path, ec);
        if (ec) {
            if (error != nullptr) {
                *error = "failed to write settings file: " + ec.message();
            }
            return false;
        }
    }

    return true;
}

} // namespace core

