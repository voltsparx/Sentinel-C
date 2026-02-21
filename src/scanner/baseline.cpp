#include "scanner.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include "hash.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#include <string>
#include <utility>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {

namespace fs = std::filesystem;

std::string g_last_baseline_error;
std::string g_last_baseline_warning;

void clear_baseline_status() {
    g_last_baseline_error.clear();
    g_last_baseline_warning.clear();
}

void tighten_file_permissions(const std::string& path) {
#ifndef _WIN32
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#else
    (void)path;
#endif
}

bool parse_entry(std::string line, core::FileEntry& entry) {
    const std::size_t p1 = line.find('\t');
    const std::size_t p2 = line.find('\t', p1 == std::string::npos ? p1 : p1 + 1);
    const std::size_t p3 = line.find('\t', p2 == std::string::npos ? p2 : p2 + 1);
    if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
        entry.path = line.substr(0, p1);
        entry.hash = line.substr(p1 + 1, p2 - p1 - 1);

        try {
            entry.size = static_cast<uintmax_t>(std::stoull(line.substr(p2 + 1, p3 - p2 - 1)));
            entry.mtime = static_cast<std::time_t>(std::stoll(line.substr(p3 + 1)));
        } catch (...) {
            return false;
        }

        return true;
    }

    // Backward compatibility with legacy "path|size|hash" format.
    const std::size_t l1 = line.find('|');
    const std::size_t l2 = line.find('|', l1 == std::string::npos ? l1 : l1 + 1);
    if (l1 == std::string::npos || l2 == std::string::npos) {
        return false;
    }

    entry.path = line.substr(0, l1);
    entry.hash = line.substr(l2 + 1);

    try {
        entry.size = static_cast<uintmax_t>(std::stoull(line.substr(l1 + 1, l2 - l1 - 1)));
        entry.mtime = 0;
    } catch (...) {
        return false;
    }

    return true;
}

bool read_seal_digest(std::string& digest, std::string& error) {
    digest.clear();
    std::ifstream in(config::BASELINE_SEAL_FILE);
    if (!in.is_open()) {
        error = "Baseline seal file not found: " + config::BASELINE_SEAL_FILE;
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("digest\t", 0) == 0) {
            digest = line.substr(7);
            break;
        }
    }

    if (digest.empty()) {
        error = "Baseline seal file is invalid: " + config::BASELINE_SEAL_FILE;
        return false;
    }
    return true;
}

bool verify_baseline_seal(std::string& error, std::string& warning) {
    error.clear();
    warning.clear();

    std::error_code ec;
    if (!fs::exists(config::BASELINE_DB, ec)) {
        error = "Baseline file not found: " + config::BASELINE_DB;
        return false;
    }

    if (!fs::exists(config::BASELINE_SEAL_FILE, ec)) {
        warning = "Baseline seal is missing. Re-run --update to enable tamper guard.";
        return true;
    }

    std::string expected_digest;
    if (!read_seal_digest(expected_digest, error)) {
        return false;
    }

    const std::string actual_digest = hash::sha256_file(config::BASELINE_DB);
    if (actual_digest.empty()) {
        error = "Failed to hash baseline during tamper verification.";
        return false;
    }

    if (actual_digest != expected_digest) {
        error =
            "Baseline tamper guard failed: seal digest mismatch. "
            "Baseline may have been modified outside Sentinel-C.";
        return false;
    }

    return true;
}

} // namespace

namespace scanner {

bool load_baseline(FileMap& baseline, std::string* baseline_root) {
    clear_baseline_status();
    baseline.clear();
    if (baseline_root != nullptr) {
        *baseline_root = "";
    }

    std::string seal_error;
    std::string seal_warning;
    if (!verify_baseline_seal(seal_error, seal_warning)) {
        g_last_baseline_error = seal_error;
        return false;
    }
    g_last_baseline_warning = seal_warning;

    std::ifstream in(config::BASELINE_DB);
    if (!in.is_open()) {
        g_last_baseline_error = "Baseline file not found: " + config::BASELINE_DB;
        return false;
    }

    bool seen_content = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line.rfind("root\t", 0) == 0) {
            if (baseline_root != nullptr) {
                *baseline_root = line.substr(5);
            }
            seen_content = true;
            continue;
        }

        if (line.rfind("generated\t", 0) == 0) {
            seen_content = true;
            continue;
        }

        if (line.rfind("file\t", 0) == 0) {
            line = line.substr(5);
        }

        core::FileEntry entry;
        if (!parse_entry(line, entry)) {
            continue;
        }
        baseline[entry.path] = std::move(entry);
        seen_content = true;
    }

    if (!seen_content) {
        g_last_baseline_error = "Baseline file is empty or invalid: " + config::BASELINE_DB;
    }
    return seen_content;
}

bool save_baseline(const FileMap& data, const std::string& baseline_root) {
    clear_baseline_status();
    std::ofstream out(config::BASELINE_DB, std::ios::trunc);
    if (!out.is_open()) {
        g_last_baseline_error = "Failed to open baseline file for write: " + config::BASELINE_DB;
        return false;
    }

    out << "# Sentinel-C baseline v2\n";
    out << "root\t" << baseline_root << "\n";
    out << "generated\t" << fsutil::timestamp() << "\n";

    for (const auto& item : data) {
        const core::FileEntry& entry = item.second;
        out << "file\t"
            << entry.path << '\t'
            << entry.hash << '\t'
            << entry.size << '\t'
            << entry.mtime << "\n";
    }
    out.close();
    if (!out) {
        g_last_baseline_error = "Failed to flush baseline file: " + config::BASELINE_DB;
        return false;
    }
    tighten_file_permissions(config::BASELINE_DB);

    const std::string digest = hash::sha256_file(config::BASELINE_DB);
    if (digest.empty()) {
        g_last_baseline_error = "Failed to hash baseline while creating seal.";
        return false;
    }

    std::ofstream seal(config::BASELINE_SEAL_FILE, std::ios::trunc);
    if (!seal.is_open()) {
        g_last_baseline_error =
            "Failed to open baseline seal file for write: " + config::BASELINE_SEAL_FILE;
        return false;
    }

    seal << "# Sentinel-C baseline seal v1\n";
    seal << "algorithm\tSHA256\n";
    seal << "created\t" << fsutil::timestamp() << "\n";
    seal << "digest\t" << digest << "\n";
    seal.close();
    if (!seal) {
        g_last_baseline_error =
            "Failed to flush baseline seal file: " + config::BASELINE_SEAL_FILE;
        return false;
    }
    tighten_file_permissions(config::BASELINE_SEAL_FILE);

    return true;
}

const std::string& baseline_last_error() {
    return g_last_baseline_error;
}

const std::string& baseline_last_warning() {
    return g_last_baseline_warning;
}

} // namespace scanner
