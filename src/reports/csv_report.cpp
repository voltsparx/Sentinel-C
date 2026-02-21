#include "csv_report.h"
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
        return "";
    }
    const std::tm tm = local_time(t);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string escape_csv(const std::string& value) {
    bool needs_quotes = false;
    for (const char ch : value) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return value;
    }

    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('"');
    return out;
}

void write_row(std::ofstream& out,
               const std::string& section,
               const std::string& kind,
               const std::string& path,
               uintmax_t size,
               const std::string& mtime,
               const std::string& hash,
               const std::string& note) {
    out << escape_csv(section) << ","
        << escape_csv(kind) << ","
        << escape_csv(path) << ","
        << size << ","
        << escape_csv(mtime) << ","
        << escape_csv(hash) << ","
        << escape_csv(note) << "\n";
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

void write_advisor_block(std::ofstream& out, const AdvisorNarrative& narrative) {
    write_row(out, "advisor", "summary", "", 0, "", "", narrative.summary);
    write_row(out, "advisor", "risk_level", "", 0, "", "", narrative.risk_level);
    for (const std::string& line : narrative.whys) {
        write_row(out, "advisor", "why", "", 0, "", "", line);
    }
    for (const std::string& line : narrative.what_matters) {
        write_row(out, "advisor", "what_matters", "", 0, "", "", line);
    }
    for (const std::string& line : narrative.teaching) {
        write_row(out, "advisor", "teaching", "", 0, "", "", line);
    }
    for (const std::string& line : narrative.next_steps) {
        write_row(out, "advisor", "next_step", "", 0, "", "", line);
    }
}

} // namespace

std::string write_csv(const scanner::ScanResult& result, const std::string& scan_id) {
    const std::string id = scan_id.empty() ? fsutil::timestamp() : scan_id;
    const std::string file =
        config::REPORT_CSV_DIR + "/sentinel-c_integrity_csv_report_" + id + ".csv";

    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open()) {
        return "";
    }

    const AdvisorNarrative narrative = advisor_narrative(result);
    const std::string status = advisor_status(result) == "clean" ? "CLEAN" : "CHANGES_DETECTED";

    out << "section,type,path,size,mtime,sha256,note\n";
    write_row(out, "summary", "status", "", 0, "", "", status);
    write_row(out, "summary", "scanned", "", result.stats.scanned, "", "", "");
    write_row(out, "summary", "added", "", result.stats.added, "", "", "");
    write_row(out, "summary", "modified", "", result.stats.modified, "", "", "");
    write_row(out, "summary", "deleted", "", result.stats.deleted, "", "", "");

    {
        std::ostringstream duration;
        duration << std::fixed << std::setprecision(3) << result.stats.duration;
        write_row(out, "summary", "duration_seconds", "", 0, "", "", duration.str());
    }

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

    for (const ChangeRow& row : rows) {
        write_row(out, "change", row.status, row.path, row.size, row.mtime, row.hash, "");
    }

    write_advisor_block(out, narrative);
    return file;
}

} // namespace reports
