#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace core {

struct FileEntry {
    std::string path;
    std::string hash;
    uintmax_t   size;
    std::time_t mtime;
};

struct ScanStats {
    size_t scanned  = 0;
    size_t added    = 0;
    size_t modified = 0;
    size_t deleted  = 0;
    double duration = 0.0;
};

struct OutputPaths {
    std::string cli_report;
    std::string html_report;
    std::string json_report;
    std::string csv_report;
    std::string log_file;
    std::string baseline;
};

} // namespace core
