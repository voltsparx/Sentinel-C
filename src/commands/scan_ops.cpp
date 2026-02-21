#include "scan_ops.h"
#include "advisor.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include "../core/logger.h"
#include "../core/summary.h"
#include "../reports/cli_report.h"
#include "../reports/csv_report.h"
#include "../reports/html_report.h"
#include "../reports/json_report.h"
#include <cctype>
#include <chrono>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>

namespace commands {

namespace {

struct ReportSelection {
    bool cli = true;
    bool html = true;
    bool json = true;
    bool csv = true;
};

bool any_enabled(const ReportSelection& selection) {
    return selection.cli || selection.html || selection.json || selection.csv;
}

ReportSelection none_enabled() {
    return ReportSelection{false, false, false, false};
}

bool parse_report_selection(const ParsedArgs& parsed,
                            ReportSelection& selection,
                            bool& explicit_selection,
                            std::string& error) {
    selection = ReportSelection{};
    explicit_selection = false;

    const auto raw_value = option_value(parsed, "report-formats");
    if (!raw_value.has_value()) {
        return true;
    }

    explicit_selection = true;
    selection = none_enabled();

    std::string token;
    std::stringstream stream(*raw_value);
    while (std::getline(stream, token, ',')) {
        const std::size_t first = token.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }
        const std::size_t last = token.find_last_not_of(" \t\r\n");
        token = token.substr(first, last - first + 1);
        if (token.empty()) {
            continue;
        }
        for (char& ch : token) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (token == "all") {
            selection = ReportSelection{};
            continue;
        }
        if (token == "none") {
            selection = none_enabled();
            continue;
        }
        if (token == "cli") {
            selection.cli = true;
            continue;
        }
        if (token == "html") {
            selection.html = true;
            continue;
        }
        if (token == "json") {
            selection.json = true;
            continue;
        }
        if (token == "csv") {
            selection.csv = true;
            continue;
        }

        error = "Invalid report format '" + token +
                "'. Use comma-separated values from: cli,html,json,csv,all,none.";
        return false;
    }

    return true;
}

std::string safe_report_result(std::future<std::string>& job,
                               const char* report_name,
                               bool log_errors) {
    try {
        return job.get();
    } catch (const std::exception& ex) {
        if (log_errors) {
            logger::error(std::string("Failed to generate ") + report_name + " report: " + ex.what());
        }
    } catch (...) {
        if (log_errors) {
            logger::error(std::string("Failed to generate ") + report_name + " report: unknown error");
        }
    }
    return "";
}

void warn_if_missing_report(const std::string& path, const char* report_name, bool log_errors) {
    if (log_errors && path.empty()) {
        logger::warning(std::string(report_name) + " report generation returned empty output path.");
    }
}

void generate_reports_async(const scanner::ScanResult& result,
                            const std::string& scan_id,
                            const ReportSelection& selection,
                            core::OutputPaths& outputs,
                            bool log_errors) {
    std::optional<std::future<std::string>> cli_job;
    std::optional<std::future<std::string>> html_job;
    std::optional<std::future<std::string>> json_job;
    std::optional<std::future<std::string>> csv_job;

    if (selection.cli) {
        cli_job.emplace(std::async(std::launch::async, [&result, &scan_id]() {
            return reports::write_cli(result, scan_id);
        }));
    }
    if (selection.html) {
        html_job.emplace(std::async(std::launch::async, [&result, &scan_id]() {
            return reports::write_html(result, scan_id);
        }));
    }
    if (selection.json) {
        json_job.emplace(std::async(std::launch::async, [&result, &scan_id]() {
            return reports::write_json(result, scan_id);
        }));
    }
    if (selection.csv) {
        csv_job.emplace(std::async(std::launch::async, [&result, &scan_id]() {
            return reports::write_csv(result, scan_id);
        }));
    }

    outputs.cli_report = selection.cli ? safe_report_result(*cli_job, "CLI", log_errors) : "N/A";
    outputs.html_report = selection.html ? safe_report_result(*html_job, "HTML", log_errors) : "N/A";
    outputs.json_report = selection.json ? safe_report_result(*json_job, "JSON", log_errors) : "N/A";
    outputs.csv_report = selection.csv ? safe_report_result(*csv_job, "CSV", log_errors) : "N/A";

    if (selection.cli) {
        warn_if_missing_report(outputs.cli_report, "CLI", log_errors);
    }
    if (selection.html) {
        warn_if_missing_report(outputs.html_report, "HTML", log_errors);
    }
    if (selection.json) {
        warn_if_missing_report(outputs.json_report, "JSON", log_errors);
    }
    if (selection.csv) {
        warn_if_missing_report(outputs.csv_report, "CSV", log_errors);
    }
}

} // namespace

ExitCode load_baseline(BaselineView& baseline, bool quiet) {
    if (!scanner::load_baseline(baseline.files, &baseline.root)) {
        const std::string detail = scanner::baseline_last_error();
        const bool baseline_missing =
            detail.find("Baseline file not found") != std::string::npos;
        const bool baseline_guard_failure =
            detail.find("seal") != std::string::npos ||
            detail.find("tamper") != std::string::npos;
        if (!quiet) {
            if (!detail.empty()) {
                logger::error(detail);
            } else {
                logger::error("Baseline not found. Run --init <path> first.");
            }
            if (baseline_guard_failure) {
                logger::error("Run --init --force or --update after confirming trusted state.");
            }
        }
        if (baseline_guard_failure) {
            return ExitCode::OperationFailed;
        }
        return baseline_missing ? ExitCode::BaselineMissing : ExitCode::OperationFailed;
    }
    const std::string warning = scanner::baseline_last_warning();
    if (!quiet && !warning.empty()) {
        logger::warning(warning);
    }
    return ExitCode::Ok;
}

ExitCode compare_target(const std::string& target,
                        ScanOutcome& outcome,
                        bool quiet,
                        bool consider_mtime) {
    BaselineView baseline;
    const ExitCode baseline_code = load_baseline(baseline, quiet);
    if (baseline_code != ExitCode::Ok) {
        return baseline_code;
    }

    if (!baseline.root.empty() && baseline.root != target) {
        if (!quiet) {
            logger::error("Baseline target mismatch.");
            logger::error("Baseline target: " + baseline.root);
            logger::error("Requested target: " + target);
        }
        return ExitCode::TargetMismatch;
    }

    core::ScanStats snapshot_stats;
    const scanner::FileMap current = scanner::build_snapshot(target, &snapshot_stats);
    outcome.result = scanner::compare(baseline.files, current, consider_mtime);
    outcome.result.stats.duration = snapshot_stats.duration;
    outcome.target = target;
    outcome.outputs = default_outputs();
    return ExitCode::Ok;
}

ExitCode handle_init(const ParsedArgs& parsed) {
    std::string raw_target;
    if (!require_single_positional(parsed, "<path>", raw_target)) {
        return ExitCode::UsageError;
    }

    if (!is_directory_path(raw_target)) {
        logger::error("Target directory does not exist: " + raw_target);
        return ExitCode::UsageError;
    }

    const bool force = has_switch(parsed, "force");
    const bool as_json = has_switch(parsed, "json");
    const bool quiet = has_switch(parsed, "quiet");
    const bool no_advice = has_switch(parsed, "no-advice");
    const std::string target = normalize_path(raw_target);

    std::error_code ec;
    const bool baseline_exists = std::filesystem::exists(config::BASELINE_DB, ec);
    if (baseline_exists && !force) {
        logger::error("Baseline already exists. Use --force to replace it.");
        return ExitCode::OperationFailed;
    }

    core::ScanStats stats;
    const scanner::FileMap snapshot = scanner::build_snapshot(target, &stats);
    if (!scanner::save_baseline(snapshot, target)) {
        const std::string detail = scanner::baseline_last_error();
        logger::error(detail.empty() ? ("Failed to save baseline: " + config::BASELINE_DB) : detail);
        return ExitCode::OperationFailed;
    }

    if (as_json) {
        std::cout << "{\n"
                  << "  \"command\": \"init\",\n"
                  << "  \"target\": \"" << json_escape(target) << "\",\n"
                  << "  \"files_scanned\": " << stats.scanned << ",\n"
                  << "  \"baseline\": \"" << json_escape(config::BASELINE_DB) << "\"\n"
                  << "}\n";
    } else {
        logger::success("Baseline initialized with " + std::to_string(stats.scanned) + " files.");
        if (!quiet) {
            core::print_summary(target, stats, default_outputs(), true);
        } else {
            logger::info("INIT summary: scanned=" + std::to_string(stats.scanned));
        }
        if (!quiet && !no_advice) {
            print_advice(build_init_advice(stats.scanned));
        }
    }

    return ExitCode::Ok;
}

ExitCode handle_scan_mode(const ParsedArgs& parsed, ScanMode mode) {
    std::string raw_target;
    if (!require_single_positional(parsed, "<path>", raw_target)) {
        return ExitCode::UsageError;
    }

    if (!is_directory_path(raw_target)) {
        logger::error("Target directory does not exist: " + raw_target);
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    const bool requested_reports = has_switch(parsed, "reports");
    const bool no_reports = has_switch(parsed, "no-reports");
    const bool strict = has_switch(parsed, "strict");
    const bool quiet = has_switch(parsed, "quiet");
    const bool no_advice = has_switch(parsed, "no-advice");
    const bool hash_only = has_switch(parsed, "hash-only");
    const std::string target = normalize_path(raw_target);

    ReportSelection report_selection;
    bool explicit_selection = false;
    std::string report_error;
    if (!parse_report_selection(parsed, report_selection, explicit_selection, report_error)) {
        logger::error(report_error);
        return ExitCode::UsageError;
    }
    if (no_reports && explicit_selection) {
        logger::error("Use either --no-reports or --report-formats, not both.");
        return ExitCode::UsageError;
    }

    ScanOutcome outcome;
    const ExitCode compare_code = compare_target(target, outcome, as_json, !hash_only);
    if (compare_code != ExitCode::Ok) {
        if (as_json) {
            std::cout << "{\n"
                      << "  \"command\": \""
                      << (mode == ScanMode::Scan ? "scan" :
                          mode == ScanMode::Update ? "update" :
                          mode == ScanMode::Status ? "status" : "verify")
                      << "\",\n"
                      << "  \"target\": \"" << json_escape(target) << "\",\n"
                      << "  \"exit_code\": " << static_cast<int>(compare_code) << "\n"
                      << "}\n";
        }
        return compare_code;
    }

    if (!as_json && !quiet) {
        log_changes(outcome.result);
    }

    bool write_reports = (mode == ScanMode::Scan || mode == ScanMode::Update) || requested_reports;
    if (mode == ScanMode::Status) {
        write_reports = false;
    }
    if (no_reports) {
        write_reports = false;
    }
    if (explicit_selection) {
        write_reports = any_enabled(report_selection);
    }

    if (write_reports) {
        const std::string scan_id = fsutil::timestamp();
        generate_reports_async(outcome.result, scan_id, report_selection, outcome.outputs, !as_json);
    }

    if (mode == ScanMode::Update) {
        if (!scanner::save_baseline(outcome.result.current, target)) {
            const std::string detail = scanner::baseline_last_error();
            logger::error(detail.empty() ? "Scan completed, but baseline update failed." : detail);
            return ExitCode::OperationFailed;
        }
        if (!as_json) {
            logger::info("Baseline refreshed.");
        }
    }

    const bool changes = has_changes(outcome.result);
    ExitCode code = ExitCode::Ok;
    if ((mode == ScanMode::Status || mode == ScanMode::Verify || strict) && changes) {
        code = ExitCode::ChangesDetected;
    }

    if (!as_json) {
        if (!quiet) {
            core::print_summary(target, outcome.result.stats, outcome.outputs, true);
        } else {
            std::cout << "Scan: scanned=" << outcome.result.stats.scanned
                      << " added=" << outcome.result.stats.added
                      << " modified=" << outcome.result.stats.modified
                      << " deleted=" << outcome.result.stats.deleted
                      << " duration=" << std::fixed << std::setprecision(2)
                      << outcome.result.stats.duration << "s\n";
        }
        if (mode == ScanMode::Status) {
            if (changes) {
                logger::warning("STATUS: CHANGES_DETECTED");
            } else {
                logger::success("STATUS: CLEAN");
            }
        }
        if (!quiet && !no_advice) {
            print_advice(build_scan_advice(outcome.result, mode, mode == ScanMode::Update));
        }
    } else {
        print_scan_json(mode == ScanMode::Scan ? "scan" :
                        mode == ScanMode::Update ? "update" :
                        mode == ScanMode::Status ? "status" : "verify",
                        outcome, code);
    }

    return code;
}

ExitCode handle_watch(const ParsedArgs& parsed) {
    std::string raw_target;
    if (!require_single_positional(parsed, "<path>", raw_target)) {
        return ExitCode::UsageError;
    }
    if (!is_directory_path(raw_target)) {
        logger::error("Target directory does not exist: " + raw_target);
        return ExitCode::UsageError;
    }

    int interval = 5;
    int cycles = 12;
    if (!parse_positive_option(parsed, "interval", 5, interval) ||
        !parse_positive_option(parsed, "cycles", 12, cycles)) {
        return ExitCode::UsageError;
    }

    const bool write_reports = has_switch(parsed, "reports");
    const bool fail_fast = has_switch(parsed, "fail-fast");
    const bool as_json = has_switch(parsed, "json");
    const bool quiet = has_switch(parsed, "quiet");
    const bool no_advice = has_switch(parsed, "no-advice");
    const bool hash_only = has_switch(parsed, "hash-only");
    const std::string target = normalize_path(raw_target);

    ReportSelection report_selection;
    bool explicit_selection = false;
    std::string report_error;
    if (!parse_report_selection(parsed, report_selection, explicit_selection, report_error)) {
        logger::error(report_error);
        return ExitCode::UsageError;
    }
    bool emit_reports = write_reports;
    if (explicit_selection) {
        emit_reports = any_enabled(report_selection);
    }

    BaselineView baseline;
    const ExitCode load_code = load_baseline(baseline, as_json);
    if (load_code != ExitCode::Ok) {
        if (as_json) {
            std::cout << "{"
                      << "\"command\":\"watch\","
                      << "\"target\":\"" << json_escape(target) << "\","
                      << "\"exit_code\":" << static_cast<int>(load_code)
                      << "}\n";
        }
        return load_code;
    }

    if (!baseline.root.empty() && baseline.root != target) {
        if (as_json) {
            std::cout << "{"
                      << "\"command\":\"watch\","
                      << "\"target\":\"" << json_escape(target) << "\","
                      << "\"exit_code\":" << static_cast<int>(ExitCode::TargetMismatch)
                      << "}\n";
        } else {
            logger::error("Baseline target mismatch.");
            logger::error("Baseline target: " + baseline.root);
            logger::error("Requested target: " + target);
        }
        return ExitCode::TargetMismatch;
    }

    bool any_changes = false;
    for (int cycle = 1; cycle <= cycles; ++cycle) {
        core::ScanStats snapshot_stats;
        const scanner::FileMap current = scanner::build_snapshot(target, &snapshot_stats);
        scanner::ScanResult result = scanner::compare(baseline.files, current, !hash_only);
        result.stats.duration = snapshot_stats.duration;
        const bool changed = has_changes(result);
        any_changes = any_changes || changed;

        if (as_json) {
            std::cout << "{"
                      << "\"cycle\":" << cycle << ","
                      << "\"cycles\":" << cycles << ","
                      << "\"scanned\":" << result.stats.scanned << ","
                      << "\"added\":" << result.stats.added << ","
                      << "\"modified\":" << result.stats.modified << ","
                      << "\"deleted\":" << result.stats.deleted << ","
                      << "\"changed\":" << (changed ? "true" : "false")
                      << "}\n";
        } else if (!quiet) {
            std::cout << "Cycle " << cycle << "/" << cycles
                      << " | scanned=" << result.stats.scanned
                      << " added=" << result.stats.added
                      << " modified=" << result.stats.modified
                      << " deleted=" << result.stats.deleted
                      << " duration=" << std::fixed << std::setprecision(2)
                      << result.stats.duration << "s\n";
        }

        if (changed) {
            if (!as_json && !quiet) {
                log_changes(result);
            }
            if (emit_reports) {
                const std::string scan_id =
                    fsutil::timestamp() + "_watch_" + std::to_string(cycle);
                core::OutputPaths watch_outputs = default_outputs();
                generate_reports_async(result, scan_id, report_selection, watch_outputs, !as_json);
            }
            if (fail_fast) {
                return ExitCode::ChangesDetected;
            }
        }

        if (cycle < cycles) {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }

    if (!as_json) {
        if (quiet) {
            std::cout << "Watch complete: cycles=" << cycles
                      << " interval=" << interval
                      << "s changes_detected=" << (any_changes ? "yes" : "no") << "\n";
        } else if (!no_advice) {
            print_advice(build_watch_advice(any_changes, cycles, interval, fail_fast));
        }
    }
    return any_changes ? ExitCode::ChangesDetected : ExitCode::Ok;
}

} // namespace commands
