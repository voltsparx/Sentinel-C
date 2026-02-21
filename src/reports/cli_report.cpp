#include "cli_report.h"
#include "advice.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace reports {

namespace {

struct ChangeRow {
    std::string status;
    std::string path;
    std::string hash;
    std::string mtime;
    uintmax_t size = 0;
};

std::tm local_time(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string format_mtime(std::time_t t) {
    if (t <= 0) {
        return "-";
    }
    const std::tm tm = local_time(t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void collect_rows(const scanner::FileMap& files,
                  const std::string& status,
                  std::vector<ChangeRow>& rows) {
    for (const auto& item : files) {
        const core::FileEntry& entry = item.second;
        rows.push_back(ChangeRow{
            status,
            entry.path,
            entry.hash,
            format_mtime(entry.mtime),
            entry.size
        });
    }
}

void write_ascii_table(std::ofstream& out, const std::vector<ChangeRow>& rows) {
    std::size_t status_w = std::string("STATUS").size();
    std::size_t size_w = std::string("SIZE").size();
    std::size_t mtime_w = std::string("MTIME").size();
    std::size_t path_w = std::string("PATH").size();
    std::size_t hash_w = std::string("SHA256").size();

    for (const ChangeRow& row : rows) {
        status_w = std::max<std::size_t>(status_w, row.status.size());
        size_w = std::max<std::size_t>(size_w, std::to_string(row.size).size());
        mtime_w = std::max<std::size_t>(mtime_w, row.mtime.size());
        path_w = std::max<std::size_t>(path_w, row.path.size());
        hash_w = std::max<std::size_t>(hash_w, row.hash.size());
    }

    const auto print_hr = [&]() {
        out << "+"
            << std::string(status_w + 2, '-')
            << "+" << std::string(size_w + 2, '-')
            << "+" << std::string(mtime_w + 2, '-')
            << "+" << std::string(path_w + 2, '-')
            << "+" << std::string(hash_w + 2, '-')
            << "+\n";
    };

    print_hr();
    out << "| " << std::left << std::setw(static_cast<int>(status_w)) << "STATUS"
        << " | " << std::right << std::setw(static_cast<int>(size_w)) << "SIZE"
        << " | " << std::left << std::setw(static_cast<int>(mtime_w)) << "MTIME"
        << " | " << std::left << std::setw(static_cast<int>(path_w)) << "PATH"
        << " | " << std::left << std::setw(static_cast<int>(hash_w)) << "SHA256"
        << " |\n";
    print_hr();

    for (const ChangeRow& row : rows) {
        out << "| " << std::left << std::setw(static_cast<int>(status_w)) << row.status
            << " | " << std::right << std::setw(static_cast<int>(size_w)) << row.size
            << " | " << std::left << std::setw(static_cast<int>(mtime_w)) << row.mtime
            << " | " << std::left << std::setw(static_cast<int>(path_w)) << row.path
            << " | " << std::left << std::setw(static_cast<int>(hash_w)) << row.hash
            << " |\n";
    }
    print_hr();
}

} // namespace

std::string write_cli(const scanner::ScanResult& result, const std::string& scan_id) {
    const std::string id = scan_id.empty() ? fsutil::timestamp() : scan_id;
    const std::string file = config::REPORT_CLI_DIR + "/scan_" + id + ".txt";

    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open()) {
        return "";
    }

    const std::string status = advisor_status(result) == "clean" ? "CLEAN" : "CHANGES_DETECTED";
    const AdvisorNarrative narrative = advisor_narrative(result);

    out << config::TOOL_NAME << " " << config::VERSION << " - CLI Scan Report\n";
    out << "==================================\n\n";
    out << "Scanned Files : " << result.stats.scanned << "\n";
    out << "New Files     : " << result.stats.added << "\n";
    out << "Modified      : " << result.stats.modified << "\n";
    out << "Deleted       : " << result.stats.deleted << "\n";
    out << "Duration      : " << std::fixed << std::setprecision(3)
        << result.stats.duration << " sec\n";
    out << "Status        : " << status << "\n\n";
    out << "Risk Level    : " << (narrative.risk_level.empty() ? "unknown" : narrative.risk_level) << "\n\n";

    std::vector<ChangeRow> rows;
    rows.reserve(result.added.size() + result.modified.size() + result.deleted.size());
    collect_rows(result.added, "NEW", rows);
    collect_rows(result.modified, "MODIFIED", rows);
    collect_rows(result.deleted, "DELETED", rows);
    std::sort(rows.begin(), rows.end(), [](const ChangeRow& left, const ChangeRow& right) {
        if (left.path != right.path) {
            return left.path < right.path;
        }
        return left.status < right.status;
    });

    out << "Change Table (ASCII)\n";
    out << "--------------------\n";
    if (rows.empty()) {
        out << "No changed files detected.\n";
    } else {
        write_ascii_table(out, rows);
    }

    out << "\nNano Advisor\n";
    out << "------------\n";
    out << " Summary:\n";
    out << "  > " << narrative.summary << "\n";

    if (!narrative.whys.empty()) {
        out << " Why this matters:\n";
        for (const std::string& line : narrative.whys) {
            out << "  - " << line << "\n";
        }
    }

    if (!narrative.what_matters.empty()) {
        out << " What matters now:\n";
        for (const std::string& line : narrative.what_matters) {
            out << "  - " << line << "\n";
        }
    }

    if (!narrative.teaching.empty()) {
        out << " Teaching notes:\n";
        for (const std::string& line : narrative.teaching) {
            out << "  - " << line << "\n";
        }
    }

    if (!narrative.next_steps.empty()) {
        out << " Suggested next steps:\n";
        for (const std::string& line : narrative.next_steps) {
            out << "  - " << line << "\n";
        }
    }

    return file;
}

} // namespace reports
