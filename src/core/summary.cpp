#include "summary.h"
#include "config.h"
#include <iostream>
#include <iomanip>
#include <ctime>

static std::string now() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

namespace core {

void print_summary(
    const std::string& target,
    const ScanStats& s,
    const OutputPaths& p,
    bool baseline_ok
) {
    std::cout <<
    "------------------------------------------------------------\n"
    << config::TOOL_NAME << " " << config::VERSION << " Scan Summary\n"
    "------------------------------------------------------------\n"
    << "Scan Time        : " << now() << "\n"
    << "Target Directory : " << target << "\n"
    << "Files Scanned    : " << s.scanned << "\n\n"
    << "New Files        : " << s.added << "\n"
    << "Modified Files   : " << s.modified << "\n"
    << "Deleted Files    : " << s.deleted << "\n\n"
    << "Scan Duration    : " << std::fixed << std::setprecision(2)
    << s.duration << " seconds\n"
    "------------------------------------------------------------\n\n"
    << "Output Locations:\n"
    << "  CLI Report  : " << p.cli_report << "\n"
    << "  HTML Report : " << p.html_report << "\n"
    << "  JSON Report : " << p.json_report << "\n"
    << "  CSV Report  : " << p.csv_report << "\n"
    << "  Log File    : " << p.log_file << "\n"
    << "  Baseline    : " << p.baseline << "\n\n";

    if (!baseline_ok) {
        std::cout <<
        "Status: Baseline integrity issue detected.\n"
        "Recommended Actions:\n"
        "  * Reinitialize baseline using --init\n"
        "  * Ensure baseline file is stored securely\n";
        return;
    }

    if (s.added == 0 && s.modified == 0 && s.deleted == 0) {
        std::cout <<
        "Status: No integrity changes detected.\n"
        "Recommended Actions:\n"
        "  * No action required\n"
        "  * Continue routine monitoring\n";
    } else {
        std::cout <<
        "Status: Integrity changes detected.\n"
        "Recommended Actions:\n"
        "  * Review reports for affected files\n"
        "  * Verify changes were intentional\n"
        "  * Update baseline if changes are legitimate\n";
    }

    std::cout << "\nScan completed successfully.\n";
}

}
