#include "json_report.h"
#include "advice.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string escape_json(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

void write_paths(std::ofstream& out,
                 const char* name,
                 const scanner::FileMap& data,
                 bool trailing_comma) {
    std::vector<std::string> paths;
    paths.reserve(data.size());
    for (const auto& item : data) {
        paths.push_back(item.first);
    }
    std::sort(paths.begin(), paths.end());

    out << "  \"" << name << "\": [\n";
    for (std::size_t index = 0; index < paths.size(); ++index) {
        out << "    \"" << escape_json(paths[index]) << "\"";
        if (index + 1 < paths.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]";
    if (trailing_comma) {
        out << ",";
    }
    out << "\n";
}

void write_string_array(std::ofstream& out,
                        const char* key,
                        const std::vector<std::string>& values,
                        bool trailing_comma) {
    out << "    \"" << key << "\": [\n";
    for (std::size_t i = 0; i < values.size(); ++i) {
        out << "      \"" << escape_json(values[i]) << "\"";
        if (i + 1 < values.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "    ]";
    if (trailing_comma) {
        out << ",";
    }
    out << "\n";
}

} // namespace

namespace reports {

std::string write_json(const scanner::ScanResult& result, const std::string& scan_id) {
    const std::string id = scan_id.empty() ? fsutil::timestamp() : scan_id;
    const std::string file = config::REPORT_JSON_DIR + "/scan_" + id + ".json";

    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open()) {
        return "";
    }

    const AdvisorNarrative narrative = advisor_narrative(result);
    const std::string status = advisor_status(result);

    out << "{\n";
    out << "  \"version\": \"" << escape_json(config::VERSION) << "\",\n";
    out << "  \"status\": \"" << status << "\",\n";
    out << "  \"stats\": {\n";
    out << "    \"scanned\": " << result.stats.scanned << ",\n";
    out << "    \"added\": " << result.stats.added << ",\n";
    out << "    \"modified\": " << result.stats.modified << ",\n";
    out << "    \"deleted\": " << result.stats.deleted << ",\n";
    out << "    \"duration\": " << result.stats.duration << "\n";
    out << "  },\n";
    write_paths(out, "new", result.added, true);
    write_paths(out, "modified", result.modified, true);
    write_paths(out, "deleted", result.deleted, true);
    out << "  \"advisor\": {\n";
    out << "    \"summary\": \"" << escape_json(narrative.summary) << "\",\n";
    out << "    \"risk_level\": \"" << escape_json(narrative.risk_level) << "\",\n";
    write_string_array(out, "whys", narrative.whys, true);
    write_string_array(out, "what_matters", narrative.what_matters, true);
    write_string_array(out, "teaching", narrative.teaching, true);
    write_string_array(out, "next_steps", narrative.next_steps, false);
    out << "  }\n";
    out << "}\n";

    return file;
}

} // namespace reports
