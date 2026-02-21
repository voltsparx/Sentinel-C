#pragma once
#include <string>
#include <unordered_map>
#include "../core/types.h"

namespace scanner {

using FileMap = std::unordered_map<std::string, core::FileEntry>;

struct ScanResult {
    core::ScanStats stats;
    FileMap current;
    FileMap added;
    FileMap modified;
    FileMap deleted;
};

FileMap build_snapshot(const std::string& target, core::ScanStats* stats = nullptr);
ScanResult compare(const FileMap& baseline, const FileMap& current);
ScanResult compare(const FileMap& baseline, const FileMap& current, bool consider_mtime);
bool load_baseline(FileMap& baseline, std::string* baseline_root = nullptr);
bool save_baseline(const FileMap& data, const std::string& baseline_root);
const std::string& baseline_last_error();
const std::string& baseline_last_warning();

}
