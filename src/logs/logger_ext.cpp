#include "../core/config.h"
#include "../scanner/scanner.h"
#include <fstream>
#include <ctime>

void log_scan_summary(const scanner::ScanResult& r) {
    std::ofstream out(config::LOG_DIR + "/scans.log", std::ios::app);
    std::time_t now = std::time(nullptr);

    out << std::ctime(&now)
        << " Scanned:" << r.stats.scanned
        << " New:" << r.stats.added
        << " Modified:" << r.stats.modified
        << " Deleted:" << r.stats.deleted
        << "\n";
}
