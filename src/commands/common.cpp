#include "common.h"
#include "../banner.h"
#include "../core/config.h"
#include "../core/logger.h"
#include "../core/metadata.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace commands {

namespace {

constexpr const char* ANSI_RESET = "\033[0m";
constexpr const char* ANSI_ORANGE = "\033[38;5;208m";
constexpr const char* ANSI_GREY = "\033[90m";
constexpr const char* ANSI_CYAN = "\033[36m";

} // namespace

bool has_changes(const scanner::ScanResult& result) {
    return result.stats.added > 0 || result.stats.modified > 0 || result.stats.deleted > 0;
}

std::string colorize(const std::string& text, const char* ansi_color) {
    if (!config::COLOR_OUTPUT) {
        return text;
    }
    return std::string(ansi_color) + text + ANSI_RESET;
}

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string normalize_path(const std::string& path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return fs::path(path).lexically_normal().generic_string();
}

bool is_directory_path(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

core::OutputPaths default_outputs() {
    core::OutputPaths outputs;
    outputs.cli_report = "N/A";
    outputs.html_report = "N/A";
    outputs.json_report = "N/A";
    outputs.csv_report = "N/A";
    outputs.log_file = config::LOG_FILE;
    outputs.baseline = config::BASELINE_DB;
    outputs.baseline_seal = config::BASELINE_SEAL_FILE;
    return outputs;
}

void print_scan_json(const std::string& command, const ScanOutcome& outcome, ExitCode code) {
    const scanner::ScanResult& result = outcome.result;
    const bool changed = has_changes(result);
    std::cout << "{\n"
              << "  \"command\": \"" << json_escape(command) << "\",\n"
              << "  \"target\": \"" << json_escape(outcome.target) << "\",\n"
              << "  \"changed\": " << (changed ? "true" : "false") << ",\n"
              << "  \"exit_code\": " << static_cast<int>(code) << ",\n"
              << "  \"stats\": {\n"
              << "    \"scanned\": " << result.stats.scanned << ",\n"
              << "    \"added\": " << result.stats.added << ",\n"
              << "    \"modified\": " << result.stats.modified << ",\n"
              << "    \"deleted\": " << result.stats.deleted << ",\n"
              << "    \"duration\": " << result.stats.duration << "\n"
              << "  },\n"
              << "  \"outputs\": {\n"
              << "    \"cli\": \"" << json_escape(outcome.outputs.cli_report) << "\",\n"
              << "    \"html\": \"" << json_escape(outcome.outputs.html_report) << "\",\n"
              << "    \"json\": \"" << json_escape(outcome.outputs.json_report) << "\",\n"
              << "    \"csv\": \"" << json_escape(outcome.outputs.csv_report) << "\"\n"
              << "  }\n"
              << "}\n";
}

void print_usage_lines() {
    std::cout
        << "Usage:\n"
        << "  sentinel-c --init <path> [--force] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --scan <path> [--report-formats list] [--strict] [--hash-only] [--quiet] [--no-advice] [--no-reports] [--json] [--output-root <path>]\n"
        << "  sentinel-c --update <path> [--report-formats list] [--strict] [--hash-only] [--quiet] [--no-advice] [--no-reports] [--json] [--output-root <path>]\n"
        << "  sentinel-c --status <path> [--hash-only] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --verify <path> [--reports] [--report-formats list] [--strict] [--hash-only] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --watch <path> [--interval N] [--cycles N] [--reports] [--report-formats list] [--fail-fast] [--hash-only] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --doctor [--fix] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --guard [--fix] [--quiet] [--no-advice] [--json] [--output-root <path>]\n"
        << "  sentinel-c --set-destination <path> [--json] [--quiet]\n"
        << "  sentinel-c --show-destination [--json] [--quiet] [--output-root <path>]\n"
        << "  sentinel-c --list-baseline [--limit N] [--json] [--output-root <path>]\n"
        << "  sentinel-c --show-baseline <path> [--json] [--output-root <path>]\n"
        << "  sentinel-c --export-baseline <file> [--overwrite] [--output-root <path>]\n"
        << "  sentinel-c --import-baseline <file> [--force] [--output-root <path>]\n"
        << "  sentinel-c --purge-reports [--days N | --all] [--dry-run] [--output-root <path>]\n"
        << "  sentinel-c --tail-log [--lines N] [--output-root <path>]\n"
        << "  sentinel-c --report-index [--type all|cli|html|json|csv] [--limit N] [--json] [--output-root <path>]\n"
        << "  sentinel-c --prompt-mode [--target <path>] [--interval N] [--cycles N] [--reports] [--report-formats list] [--strict] [--hash-only] [--quiet] [--no-advice] [--output-root <path>]\n"
        << "  sentinel-c --version [--json]\n"
        << "  sentinel-c --about\n"
        << "  sentinel-c --explain\n"
        << "  sentinel-c --help\n\n"
        << "Storage Default:\n"
        << "  Logs and reports are stored under the binary directory by default.\n"
        << "  Use --set-destination <path> to save a persistent destination for future runs.\n\n";
}

void print_no_command_hint() {
    std::cout
        << "No command was provided.\n"
        << "Try: sentinel-c --help | sentinel-c --about | sentinel-c --prompt-mode\n"
        << "Common: sentinel-c --init <path> | sentinel-c --scan <path> | sentinel-c --status <path>\n";
}

void print_help() {
    show_banner();
    std::cout << colorize("Trust model: local-first; no automatic data upload.", ANSI_CYAN) << "\n"
              << colorize("Use only on systems you own or are authorized to monitor.", ANSI_GREY)
              << "\n\n";

    print_usage_lines();
    std::cout
        << "Exit Codes:\n"
        << "  0 = success\n"
        << "  1 = usage/argument error\n"
        << "  2 = integrity changes detected\n"
        << "  3 = baseline missing\n"
        << "  4 = baseline target mismatch\n"
        << "  5 = operation failed\n";
}

void print_version(bool as_json) {
    if (as_json) {
        std::cout << "{\n"
                  << "  \"tool\": \"" << config::TOOL_NAME << "\",\n"
                  << "  \"version\": \"" << config::VERSION << "\",\n"
                  << "  \"author\": \"" << json_escape(metadata::AUTHOR) << "\",\n"
                  << "  \"contact\": \"" << json_escape(metadata::CONTACT) << "\"\n"
                  << "}\n";
        return;
    }

    std::cout << config::TOOL_NAME << " " << config::VERSION << "\n"
              << "By: " << colorize(metadata::AUTHOR, ANSI_ORANGE) << "\n"
              << "Contact: " << colorize(metadata::CONTACT, ANSI_GREY) << "\n";
}

void print_about() {
    show_banner();
    std::cout
        << "Sentinel-C is a host-based integrity defense framework focused on\n"
        << "clear evidence, predictable behavior, and local-first operation.\n\n"
        << "What it is designed for:\n"
        << "  - Baseline and integrity drift detection\n"
        << "  - Human-readable and machine-readable reporting (CLI/HTML/JSON/CSV)\n"
        << "  - Reliable CLI operations for both manual and CI workflows\n"
        << "  - Guided prompt mode for beginner-friendly operations\n\n"
        << "Prompt keywords:\n"
        << "  - banner : clear screen and print Sentinel-C banner\n"
        << "  - clear  : clear console screen\n"
        << "  - exit   : leave prompt mode (Ctrl+C also exits)\n\n"
        << "Output destination:\n"
        << "  - Default: binary directory/sentinel-c-logs\n"
        << "  - Override per command with --output-root <path>\n"
        << "  - Save persistent destination with --set-destination <path>\n\n"
        << "Trust posture:\n"
        << "  - Runs locally and does not auto-upload data\n"
        << "  - Uses explicit commands for state-changing operations\n"
        << "  - Favors transparent output and explicit exit codes\n\n"
        << "Friendly reminder: use it only on systems you own or are authorized to monitor.\n";
}

void print_explain() {
    std::cout
        << "Major Commands (10) with sub-flags and examples\n"
        << "-----------------------------------------------\n\n"
        << "1. --init <path>\n"
        << "   Purpose: create a trusted baseline snapshot.\n"
        << "   Sub-flags: --force, --quiet, --no-advice, --json\n"
        << "   Example: sentinel-c --init C:\\\\Work\\\\Target --force\n\n"
        << "2. --scan <path>\n"
        << "   Purpose: compare current state with baseline and generate reports.\n"
        << "   Sub-flags: --report-formats <list>, --strict, --hash-only, --quiet, --no-advice, --no-reports, --json\n"
        << "   Example: sentinel-c --scan C:\\\\Work\\\\Target --report-formats cli,html,csv --strict\n\n"
        << "3. --update <path>\n"
        << "   Purpose: scan, then refresh baseline after approved changes.\n"
        << "   Sub-flags: --report-formats <list>, --strict, --hash-only, --quiet, --no-advice, --no-reports, --json\n"
        << "   Example: sentinel-c --update C:\\\\Work\\\\Target --report-formats all\n\n"
        << "4. --status <path>\n"
        << "   Purpose: CI-friendly integrity check with exit codes.\n"
        << "   Sub-flags: --hash-only, --quiet, --no-advice, --json\n"
        << "   Example: sentinel-c --status C:\\\\Work\\\\Target\n\n"
        << "5. --verify <path>\n"
        << "   Purpose: strict verification flow, optional report emission.\n"
        << "   Sub-flags: --reports, --report-formats <list>, --strict, --hash-only, --quiet, --no-advice, --json\n"
        << "   Example: sentinel-c --verify C:\\\\Work\\\\Target --report-formats json,csv\n\n"
        << "6. --watch <path>\n"
        << "   Purpose: repeated monitoring loops.\n"
        << "   Sub-flags: --interval <sec>, --cycles <n>, --reports, --report-formats <list>, --fail-fast, --hash-only, --quiet, --no-advice, --json\n"
        << "   Example: sentinel-c --watch C:\\\\Work\\\\Target --interval 10 --cycles 12\n\n"
        << "7. --doctor\n"
        << "   Purpose: check operational health of directories, log/report access, hash engine.\n"
        << "   Sub-flags: --fix, --quiet, --no-advice, --json\n"
        << "   Example: sentinel-c --doctor --fix\n\n"
        << "   Related: --guard for security-focused hardening and baseline tamper checks.\n\n"
        << "8. --list-baseline\n"
        << "   Purpose: list tracked baseline entries.\n"
        << "   Sub-flags: --limit <n>, --json\n"
        << "   Example: sentinel-c --list-baseline --limit 20\n\n"
        << "9. --show-baseline <path>\n"
        << "   Purpose: inspect one baseline record.\n"
        << "   Sub-flags: --json\n"
        << "   Example: sentinel-c --show-baseline C:\\\\Work\\\\Target\\\\a.txt\n\n"
        << "10. --purge-reports\n"
        << "    Purpose: maintenance cleanup of report artifacts.\n"
        << "    Sub-flags: --days <n>, --all, --dry-run\n"
        << "    Example: sentinel-c --purge-reports --days 30 --dry-run\n\n"
        << "Additional utility flags:\n"
        << "  - --set-destination <path> [--json] [--quiet]\n"
        << "  - --show-destination [--json] [--quiet]\n"
        << "  - --guard [--fix] [--quiet] [--no-advice] [--json]\n"
        << "  - --export-baseline <file> [--overwrite]\n"
        << "  - --import-baseline <file> [--force]\n"
        << "  - --tail-log [--lines N]\n"
        << "  - --report-index [--type all|cli|html|json|csv] [--limit N] [--json]\n"
        << "  - --output-root <path> (set logs/reports/baseline destination for current command)\n"
        << "  - --prompt-mode [--target <path>] [--interval N] [--cycles N] [--reports] [--report-formats list] [--strict] [--hash-only]\n"
        << "      Prompt keywords: banner, clear, exit; prompt set command: set destination <path>\n"
        << "  - --version [--json]\n"
        << "  - --about\n"
        << "  - --explain\n";
}

bool parse_positive_option(const ParsedArgs& parsed,
                           const std::string& name,
                           int default_value,
                           int& out_value) {
    const auto value = option_value(parsed, name);
    if (!value.has_value()) {
        out_value = default_value;
        return true;
    }

    int parsed_int = 0;
    if (!parse_positive_int(*value, parsed_int)) {
        logger::error("Invalid value for --" + name + ": " + *value);
        return false;
    }

    out_value = parsed_int;
    return true;
}

bool require_single_positional(const ParsedArgs& parsed,
                               const std::string& expected_label,
                               std::string& out_value) {
    if (parsed.positionals.empty()) {
        logger::error("Missing required argument: " + expected_label);
        return false;
    }
    if (parsed.positionals.size() > 1) {
        logger::error("Unexpected extra argument: " + parsed.positionals[1]);
        return false;
    }
    out_value = parsed.positionals.front();
    return true;
}

bool reject_positionals(const ParsedArgs& parsed) {
    if (!parsed.positionals.empty()) {
        logger::error("This command does not accept positional arguments.");
        return false;
    }
    return true;
}

bool validate_known_options(const ParsedArgs& parsed,
                            const StringSet& allowed_switches,
                            const StringSet& allowed_options) {
    for (const std::string& key : parsed.switches) {
        if (allowed_switches.find(key) == allowed_switches.end()) {
            logger::error("Unknown switch for " + parsed.command + ": --" + key);
            return false;
        }
    }
    for (const auto& item : parsed.options) {
        if (allowed_options.find(item.first) == allowed_options.end()) {
            logger::error("Unknown option for " + parsed.command + ": --" + item.first);
            return false;
        }
    }
    return true;
}

void log_changes(const scanner::ScanResult& result) {
    for (const auto& item : result.added) {
        logger::success("[NEW] " + item.first);
    }
    for (const auto& item : result.modified) {
        logger::warning("[MODIFIED] " + item.first);
    }
    for (const auto& item : result.deleted) {
        logger::error("[DELETED] " + item.first);
    }
}

} // namespace commands
