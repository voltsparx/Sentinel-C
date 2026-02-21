#include "scanner.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include <fstream>
#include <string>
#include <utility>

namespace {

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

} // namespace

namespace scanner {

bool load_baseline(FileMap& baseline, std::string* baseline_root) {
    baseline.clear();
    if (baseline_root != nullptr) {
        *baseline_root = "";
    }

    std::ifstream in(config::BASELINE_DB);
    if (!in.is_open()) {
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

    return seen_content;
}

bool save_baseline(const FileMap& data, const std::string& baseline_root) {
    std::ofstream out(config::BASELINE_DB, std::ios::trunc);
    if (!out.is_open()) {
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
    return true;
}

} // namespace scanner
