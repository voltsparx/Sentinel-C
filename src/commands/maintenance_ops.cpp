#include "maintenance_ops.h"
#include "advisor.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include "../core/logger.h"
#include "../core/runtime_settings.h"
#include "../scanner/hash.h"
#include "../scanner/scanner.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace commands {

namespace {

constexpr const char* ANSI_GREEN = "\033[32m";
constexpr const char* ANSI_YELLOW = "\033[33m";
constexpr const char* ANSI_RED = "\033[31m";
constexpr const char* ANSI_CYAN = "\033[36m";

struct ReportItem {
    std::string type;
    std::string path;
    std::uintmax_t size = 0;
    std::time_t modified = 0;
};

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::chrono::system_clock::time_point to_system_clock(const fs::file_time_type& file_time) {
    using namespace std::chrono;
    const auto system_now = std::chrono::system_clock::now();
    const auto file_now = fs::file_time_type::clock::now();
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - file_now + system_now
    );
}

std::string format_time(std::time_t timestamp) {
    if (timestamp <= 0) {
        return "-";
    }
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &timestamp);
#else
    localtime_r(&timestamp, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string report_dir_for_type(const std::string& type) {
    if (type == "cli") {
        return config::REPORT_CLI_DIR;
    }
    if (type == "html") {
        return config::REPORT_HTML_DIR;
    }
    if (type == "json") {
        return config::REPORT_JSON_DIR;
    }
    if (type == "csv") {
        return config::REPORT_CSV_DIR;
    }
    return "";
}

bool is_valid_report_type(const std::string& type) {
    return type == "all" || type == "cli" || type == "html" || type == "json" || type == "csv";
}

std::string latest_log_file_path() {
    std::error_code ec;
    if (!fs::exists(config::LOG_DIR, ec)) {
        return config::LOG_FILE;
    }

    fs::path latest_non_empty;
    fs::path latest_any;
    fs::file_time_type latest_non_empty_time{};
    fs::file_time_type latest_any_time{};
    bool have_non_empty = false;
    bool have_any = false;

    for (const auto& entry : fs::directory_iterator(config::LOG_DIR, ec)) {
        if (ec || !entry.is_regular_file()) {
            ec.clear();
            continue;
        }

        const std::string name = entry.path().filename().generic_string();
        if (!starts_with(name, "sentinel-c_activity_log_") || entry.path().extension() != ".log") {
            continue;
        }

        const fs::file_time_type write_time = entry.last_write_time(ec);
        if (ec) {
            ec.clear();
            continue;
        }

        if (!have_any || write_time > latest_any_time) {
            latest_any = entry.path();
            latest_any_time = write_time;
            have_any = true;
        }

        const std::uintmax_t size = entry.file_size(ec);
        if (ec) {
            ec.clear();
            continue;
        }

        if (size > 0 && (!have_non_empty || write_time > latest_non_empty_time)) {
            latest_non_empty = entry.path();
            latest_non_empty_time = write_time;
            have_non_empty = true;
        }
    }

    if (have_non_empty) {
        return normalize_path(latest_non_empty.generic_string());
    }
    if (have_any) {
        return normalize_path(latest_any.generic_string());
    }
    return config::LOG_FILE;
}

#ifndef _WIN32
bool others_writable(const std::string& path) {
    std::error_code ec;
    const auto perms = fs::status(path, ec).permissions();
    if (ec) {
        return false;
    }
    return (perms & fs::perms::others_write) != fs::perms::none;
}
#endif

} // namespace

ExitCode handle_set_destination(const ParsedArgs& parsed) {
    std::string destination;
    if (!require_single_positional(parsed, "<path>", destination)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    const bool quiet = has_switch(parsed, "quiet");

    std::string error;
    if (!config::set_output_root(destination, &error)) {
        if (as_json) {
            std::cout << "{\n"
                      << "  \"command\": \"set-destination\",\n"
                      << "  \"ok\": false,\n"
                      << "  \"error\": \"" << json_escape(error) << "\"\n"
                      << "}\n";
        } else {
            logger::error("Failed to set destination: " + error);
        }
        return ExitCode::UsageError;
    }

    fsutil::ensure_dirs();
    logger::reopen();

    if (!core::save_output_root(config::output_root(), &error)) {
        if (as_json) {
            std::cout << "{\n"
                      << "  \"command\": \"set-destination\",\n"
                      << "  \"ok\": false,\n"
                      << "  \"error\": \"" << json_escape(error) << "\"\n"
                      << "}\n";
        } else {
            logger::error("Destination applied but failed to persist: " + error);
        }
        return ExitCode::OperationFailed;
    }

    if (as_json) {
        std::cout << "{\n"
                  << "  \"command\": \"set-destination\",\n"
                  << "  \"ok\": true,\n"
                  << "  \"output_root\": \"" << json_escape(config::output_root()) << "\",\n"
                  << "  \"settings_file\": \"" << json_escape(core::settings_file_path()) << "\"\n"
                  << "}\n";
    } else if (!quiet) {
        logger::success("Destination saved.");
        logger::info("Output root: " + config::output_root());
        logger::info("Settings file: " + core::settings_file_path());
    }
    return ExitCode::Ok;
}

ExitCode handle_show_destination(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    const bool quiet = has_switch(parsed, "quiet");
    std::string load_error;
    const auto saved = core::load_saved_output_root(&load_error);

    if (as_json) {
        std::cout << "{\n"
                  << "  \"command\": \"show-destination\",\n"
                  << "  \"active_output_root\": \"" << json_escape(config::output_root()) << "\",\n"
                  << "  \"settings_file\": \"" << json_escape(core::settings_file_path()) << "\",\n";
        if (!load_error.empty()) {
            std::cout << "  \"warning\": \"" << json_escape(load_error) << "\",\n";
        }
        if (saved.has_value()) {
            std::cout << "  \"saved_output_root\": \"" << json_escape(*saved) << "\"\n";
        } else {
            std::cout << "  \"saved_output_root\": null\n";
        }
        std::cout << "}\n";
        return ExitCode::Ok;
    }

    if (!quiet) {
        std::cout << "Destination Settings\n"
                  << "  active output root : " << config::output_root() << "\n"
                  << "  settings file      : " << core::settings_file_path() << "\n"
                  << "  saved output root  : " << (saved.has_value() ? *saved : "(not set)") << "\n";
    }
    if (!load_error.empty()) {
        logger::warning("Settings warning: " + load_error);
    }
    return ExitCode::Ok;
}

ExitCode handle_purge_reports(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool remove_all = has_switch(parsed, "all");
    const bool dry_run = has_switch(parsed, "dry-run");
    const bool has_days = option_value(parsed, "days").has_value();
    if (remove_all && has_days) {
        logger::error("Use either --all or --days <n>, not both.");
        return ExitCode::UsageError;
    }

    int days = 30;
    if (!remove_all && !parse_positive_option(parsed, "days", 30, days)) {
        return ExitCode::UsageError;
    }

    const auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
    const std::vector<std::string> report_dirs = {
        config::REPORT_CLI_DIR,
        config::REPORT_HTML_DIR,
        config::REPORT_JSON_DIR,
        config::REPORT_CSV_DIR
    };

    std::uintmax_t matched = 0;
    std::uintmax_t removed = 0;

    for (const std::string& dir : report_dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) {
            continue;
        }

        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec || !entry.is_regular_file()) {
                ec.clear();
                continue;
            }

            bool should_remove = remove_all;
            if (!remove_all) {
                const auto last_write = to_system_clock(entry.last_write_time(ec));
                if (!ec && last_write < cutoff) {
                    should_remove = true;
                }
            }

            if (!should_remove) {
                continue;
            }

            ++matched;
            if (!dry_run) {
                fs::remove(entry.path(), ec);
                if (!ec) {
                    ++removed;
                } else {
                    ec.clear();
                }
            }
        }
    }

    if (dry_run) {
        logger::info("Dry run complete. Candidate files: " + std::to_string(matched));
    } else {
        logger::success("Report cleanup complete. Removed files: " + std::to_string(removed));
    }
    return ExitCode::Ok;
}

ExitCode handle_tail_log(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    int lines = 40;
    if (!parse_positive_option(parsed, "lines", 40, lines)) {
        return ExitCode::UsageError;
    }

    const std::string log_path = latest_log_file_path();
    std::ifstream in(log_path);
    if (!in.is_open()) {
        logger::error("Log file not found: " + log_path);
        return ExitCode::OperationFailed;
    }

    std::vector<std::string> all_lines;
    std::string line;
    while (std::getline(in, line)) {
        all_lines.push_back(line);
    }

    const std::size_t start =
        all_lines.size() > static_cast<std::size_t>(lines)
            ? all_lines.size() - static_cast<std::size_t>(lines)
            : 0;
    for (std::size_t i = start; i < all_lines.size(); ++i) {
        std::cout << all_lines[i] << "\n";
    }
    return ExitCode::Ok;
}

ExitCode handle_doctor(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    const bool fix = has_switch(parsed, "fix");
    const bool quiet = has_switch(parsed, "quiet");
    const bool no_advice = has_switch(parsed, "no-advice");
    if (fix) {
        fsutil::ensure_dirs();
    }

    std::vector<DoctorCheck> checks;
    auto push_check = [&checks](const std::string& name,
                                const std::string& level,
                                const std::string& detail) {
        checks.push_back(DoctorCheck{name, detail, level});
    };

    std::error_code ec;
    push_check("data_dir", fs::exists(config::DATA_DIR, ec) ? "pass" : "fail",
               config::DATA_DIR);
    push_check("log_dir", fs::exists(config::LOG_DIR, ec) ? "pass" : "fail",
               config::LOG_DIR);
    push_check("reports_cli_dir", fs::exists(config::REPORT_CLI_DIR, ec) ? "pass" : "fail",
               config::REPORT_CLI_DIR);
    push_check("reports_html_dir", fs::exists(config::REPORT_HTML_DIR, ec) ? "pass" : "fail",
               config::REPORT_HTML_DIR);
    push_check("reports_json_dir", fs::exists(config::REPORT_JSON_DIR, ec) ? "pass" : "fail",
               config::REPORT_JSON_DIR);
    push_check("reports_csv_dir", fs::exists(config::REPORT_CSV_DIR, ec) ? "pass" : "fail",
               config::REPORT_CSV_DIR);

    {
        std::ofstream log_stream(config::LOG_FILE, std::ios::app);
        push_check("log_writable", log_stream.is_open() ? "pass" : "fail",
                   config::LOG_FILE);
    }

    {
        bool report_write_ok = true;
        const std::vector<std::string> dirs = {
            config::REPORT_CLI_DIR, config::REPORT_HTML_DIR, config::REPORT_JSON_DIR, config::REPORT_CSV_DIR
        };
        for (const std::string& dir : dirs) {
            const fs::path tmp = fs::path(dir) / (".doctor_" + fsutil::timestamp() + ".tmp");
            std::ofstream out(tmp.string(), std::ios::trunc);
            if (!out.is_open()) {
                report_write_ok = false;
                break;
            }
            out << "ok";
            out.close();
            fs::remove(tmp, ec);
        }
        push_check("reports_writable", report_write_ok ? "pass" : "fail",
                   "report directories write test");
    }

    {
        scanner::FileMap baseline;
        std::string baseline_root;
        if (scanner::load_baseline(baseline, &baseline_root)) {
            const std::string warning = scanner::baseline_last_warning();
            push_check("baseline", warning.empty() ? "pass" : "warn",
                       warning.empty()
                           ? (baseline_root.empty() ? "baseline found" : baseline_root)
                           : warning);
        } else {
            const std::string detail = scanner::baseline_last_error();
            if (detail.find("not found") != std::string::npos ||
                detail.find("Not found") != std::string::npos) {
                push_check("baseline", "warn", "baseline missing; run --init");
            } else {
                push_check("baseline", "fail",
                           detail.empty() ? "baseline verification failed" : detail);
            }
        }
    }

    {
        if (fs::exists(config::IGNORE_FILE, ec) || fs::exists("src/.sentinelignore", ec)) {
            push_check("ignore_file", "pass", "ignore rules detected");
        } else {
            push_check("ignore_file", "warn", "no ignore file found");
        }
    }

    {
        const fs::path tmp_file =
            fs::path(config::DATA_DIR) / (".doctor_hash_" + fsutil::timestamp() + ".tmp");
        std::ofstream out(tmp_file.string(), std::ios::trunc);
        out << "sentinel-integrity";
        out.close();
        const std::string digest = hash::sha256_file(tmp_file.string());
        fs::remove(tmp_file, ec);
        push_check("hash_engine", digest.empty() ? "fail" : "pass",
                   digest.empty() ? "sha256 failed" : "sha256 operational");
    }

    std::size_t pass_count = 0;
    std::size_t warn_count = 0;
    std::size_t fail_count = 0;
    for (const DoctorCheck& check : checks) {
        if (check.level == "pass") ++pass_count;
        else if (check.level == "warn") ++warn_count;
        else ++fail_count;
    }

    if (as_json) {
        std::cout << "{\n"
                  << "  \"tool\": \"" << config::TOOL_NAME << "\",\n"
                  << "  \"version\": \"" << config::VERSION << "\",\n"
                  << "  \"pass\": " << pass_count << ",\n"
                  << "  \"warn\": " << warn_count << ",\n"
                  << "  \"fail\": " << fail_count << ",\n"
                  << "  \"checks\": [\n";
        for (std::size_t i = 0; i < checks.size(); ++i) {
            const DoctorCheck& check = checks[i];
            std::cout << "    {\"name\":\"" << json_escape(check.name)
                      << "\",\"level\":\"" << check.level
                      << "\",\"detail\":\"" << json_escape(check.detail) << "\"}";
            if (i + 1 < checks.size()) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        if (!quiet) {
            std::cout << colorize("Sentinel-C Doctor Report", ANSI_CYAN) << "\n";
            for (const DoctorCheck& check : checks) {
                std::string label = "[PASS]";
                const char* color = ANSI_GREEN;
                if (check.level == "warn") {
                    label = "[WARN]";
                    color = ANSI_YELLOW;
                } else if (check.level == "fail") {
                    label = "[FAIL]";
                    color = ANSI_RED;
                }

                std::cout << colorize(label, color) << " "
                          << std::left << std::setw(20) << check.name
                          << " " << check.detail << "\n";
            }
        }
        std::cout << "\nSummary: "
                  << pass_count << " pass, "
                  << warn_count << " warn, "
                  << fail_count << " fail\n";
        if (!quiet && !no_advice) {
            print_advice(build_doctor_advice(pass_count, warn_count, fail_count));
        }
    }

    if (fail_count > 0) {
        return ExitCode::OperationFailed;
    }
    return ExitCode::Ok;
}

ExitCode handle_guard(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    const bool fix = has_switch(parsed, "fix");
    const bool quiet = has_switch(parsed, "quiet");
    const bool no_advice = has_switch(parsed, "no-advice");

    if (fix) {
        fsutil::ensure_dirs();
    }

    std::vector<DoctorCheck> checks;
    auto push_check = [&checks](const std::string& name,
                                const std::string& level,
                                const std::string& detail) {
        checks.push_back(DoctorCheck{name, detail, level});
    };

    std::error_code ec;
    push_check("output_root", fs::exists(config::ROOT_DIR, ec) ? "pass" : "fail", config::ROOT_DIR);

#ifndef _WIN32
    if (fs::exists(config::ROOT_DIR, ec)) {
        push_check("output_root_permissions",
                   others_writable(config::ROOT_DIR) ? "warn" : "pass",
                   others_writable(config::ROOT_DIR)
                       ? "output root is writable by other users"
                       : "output root permissions are restricted");
    }
#else
    push_check("output_root_permissions", "pass", "permission check not required on this platform");
#endif

    scanner::FileMap baseline;
    std::string baseline_root;
    if (!scanner::load_baseline(baseline, &baseline_root)) {
        const std::string detail = scanner::baseline_last_error();
        if (detail.find("not found") != std::string::npos ||
            detail.find("Not found") != std::string::npos) {
            push_check("baseline", "warn", "baseline missing; run --init");
        } else {
            push_check("baseline_integrity", "fail", detail.empty() ? "baseline verification failed" : detail);
        }
    } else {
        const std::string warning = scanner::baseline_last_warning();
        push_check("baseline_integrity", warning.empty() ? "pass" : "warn",
                   warning.empty() ? "baseline seal verified" : warning);
    }

    const fs::path log_path(config::LOG_FILE);
    const std::string log_name = log_path.filename().generic_string();
    const bool log_name_ok =
        starts_with(log_name, "sentinel-c_activity_log_") && log_path.extension() == ".log";
    push_check("log_naming", log_name_ok ? "pass" : "warn",
               log_name_ok ? log_name : "log file naming pattern is not timestamped");

    if (fs::exists(config::IGNORE_FILE, ec) || fs::exists("src/.sentinelignore", ec)) {
        push_check("ignore_rules", "pass", "ignore rules detected");
    } else {
        push_check("ignore_rules", "warn", "ignore file missing");
    }

    {
        const fs::path tmp_file =
            fs::path(config::DATA_DIR) / (".guard_hash_" + fsutil::timestamp() + ".tmp");
        std::ofstream out(tmp_file.string(), std::ios::trunc);
        out << "guard-check";
        out.close();
        const std::string digest = hash::sha256_file(tmp_file.string());
        fs::remove(tmp_file, ec);
        push_check("hash_engine", digest.empty() ? "fail" : "pass",
                   digest.empty() ? "sha256 failed" : "sha256 operational");
    }

    std::size_t pass_count = 0;
    std::size_t warn_count = 0;
    std::size_t fail_count = 0;
    for (const DoctorCheck& check : checks) {
        if (check.level == "pass") ++pass_count;
        else if (check.level == "warn") ++warn_count;
        else ++fail_count;
    }

    if (as_json) {
        std::cout << "{\n"
                  << "  \"command\": \"guard\",\n"
                  << "  \"pass\": " << pass_count << ",\n"
                  << "  \"warn\": " << warn_count << ",\n"
                  << "  \"fail\": " << fail_count << ",\n"
                  << "  \"checks\": [\n";
        for (std::size_t i = 0; i < checks.size(); ++i) {
            const DoctorCheck& check = checks[i];
            std::cout << "    {\"name\":\"" << json_escape(check.name)
                      << "\",\"level\":\"" << check.level
                      << "\",\"detail\":\"" << json_escape(check.detail) << "\"}";
            if (i + 1 < checks.size()) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
    } else {
        if (!quiet) {
            std::cout << colorize("Sentinel-C Guard Report", ANSI_CYAN) << "\n";
            for (const DoctorCheck& check : checks) {
                std::string label = "[PASS]";
                const char* color = ANSI_GREEN;
                if (check.level == "warn") {
                    label = "[WARN]";
                    color = ANSI_YELLOW;
                } else if (check.level == "fail") {
                    label = "[FAIL]";
                    color = ANSI_RED;
                }

                std::cout << colorize(label, color) << " "
                          << std::left << std::setw(24) << check.name
                          << " " << check.detail << "\n";
            }
        }
        std::cout << "\nGuard Summary: "
                  << pass_count << " pass, "
                  << warn_count << " warn, "
                  << fail_count << " fail\n";
        if (!quiet && !no_advice) {
            std::vector<std::string> advice;
            if (fail_count == 0 && warn_count == 0) {
                advice.push_back("Security guard checks passed.");
                advice.push_back("Baseline seal and output paths look healthy.");
            } else if (fail_count == 0) {
                advice.push_back("Guard checks reported warnings.");
                advice.push_back("Please resolve warnings to improve hardening.");
            } else {
                advice.push_back("Guard checks reported failures.");
                advice.push_back("Please resolve failures before trusting scan outcomes.");
            }
            print_advice(advice);
        }
    }

    return fail_count > 0 ? ExitCode::OperationFailed : ExitCode::Ok;
}

ExitCode handle_report_index(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    int limit = 30;
    if (!parse_positive_option(parsed, "limit", 30, limit)) {
        return ExitCode::UsageError;
    }

    std::string type = "all";
    const auto requested_type = option_value(parsed, "type");
    if (requested_type.has_value()) {
        type = *requested_type;
        std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }

    if (!is_valid_report_type(type)) {
        logger::error("Invalid --type value. Use one of: all, cli, html, json, csv.");
        return ExitCode::UsageError;
    }

    std::vector<std::string> types;
    if (type == "all") {
        types = {"cli", "html", "json", "csv"};
    } else {
        types = {type};
    }

    std::vector<ReportItem> items;
    std::error_code ec;
    for (const std::string& item_type : types) {
        const std::string dir = report_dir_for_type(item_type);
        if (dir.empty()) {
            continue;
        }
        if (!fs::exists(dir, ec)) {
            ec.clear();
            continue;
        }

        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec || !entry.is_regular_file()) {
                ec.clear();
                continue;
            }

            ReportItem item;
            item.type = item_type;
            item.path = normalize_path(entry.path().generic_string());
            item.size = entry.file_size(ec);
            if (ec) {
                item.size = 0;
                ec.clear();
            }

            const auto last_write = to_system_clock(entry.last_write_time(ec));
            if (!ec) {
                item.modified = std::chrono::system_clock::to_time_t(last_write);
            } else {
                item.modified = 0;
                ec.clear();
            }
            items.push_back(std::move(item));
        }
    }

    std::sort(items.begin(), items.end(), [](const ReportItem& left, const ReportItem& right) {
        if (left.modified != right.modified) {
            return left.modified > right.modified;
        }
        return left.path < right.path;
    });

    if (items.size() > static_cast<std::size_t>(limit)) {
        items.resize(static_cast<std::size_t>(limit));
    }

    if (as_json) {
        std::cout << "{\n"
                  << "  \"type\": \"" << type << "\",\n"
                  << "  \"count\": " << items.size() << ",\n"
                  << "  \"items\": [\n";
        for (std::size_t i = 0; i < items.size(); ++i) {
            const ReportItem& item = items[i];
            std::cout << "    {\"type\":\"" << item.type
                      << "\",\"path\":\"" << json_escape(item.path)
                      << "\",\"size\":" << item.size
                      << ",\"modified\":\"" << json_escape(format_time(item.modified)) << "\"}";
            if (i + 1 < items.size()) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
        return ExitCode::Ok;
    }

    std::cout << "Recent Reports (" << type << ")\n";
    std::cout << "Type   Size(bytes)   Modified             Path\n";
    std::cout << "-----  -----------   -------------------  ----\n";
    for (const ReportItem& item : items) {
        std::cout << std::left << std::setw(5) << item.type << "  "
                  << std::right << std::setw(11) << item.size << "   "
                  << std::left << std::setw(19) << format_time(item.modified) << "  "
                  << item.path << "\n";
    }
    if (items.empty()) {
        std::cout << "(no reports found)\n";
    }
    return ExitCode::Ok;
}

} // namespace commands
