#pragma once

#include "arg_parser.h"
#include "../core/types.h"
#include "../scanner/scanner.h"
#include <string>
#include <unordered_set>

namespace commands {

enum class ExitCode : int {
    Ok = 0,
    UsageError = 1,
    ChangesDetected = 2,
    BaselineMissing = 3,
    TargetMismatch = 4,
    OperationFailed = 5
};

enum class ScanMode {
    Scan,
    Update,
    Status,
    Verify
};

struct BaselineView {
    scanner::FileMap files;
    std::string root;
};

struct ScanOutcome {
    scanner::ScanResult result;
    core::OutputPaths outputs;
    std::string target;
};

struct DoctorCheck {
    std::string name;
    std::string detail;
    std::string level;
};

using StringSet = std::unordered_set<std::string>;

bool has_changes(const scanner::ScanResult& result);
std::string colorize(const std::string& text, const char* ansi_color);
std::string json_escape(const std::string& value);
std::string normalize_path(const std::string& path);
bool is_directory_path(const std::string& path);
core::OutputPaths default_outputs();

void print_scan_json(const std::string& command, const ScanOutcome& outcome, ExitCode code);
void print_no_command_hint();
void print_usage_lines();
void print_help();
void print_version(bool as_json);
void print_about();
void print_explain();

bool parse_positive_option(const ParsedArgs& parsed,
                           const std::string& name,
                           int default_value,
                           int& out_value);
bool require_single_positional(const ParsedArgs& parsed,
                               const std::string& expected_label,
                               std::string& out_value);
bool reject_positionals(const ParsedArgs& parsed);
bool validate_known_options(const ParsedArgs& parsed,
                            const StringSet& allowed_switches,
                            const StringSet& allowed_options);

void log_changes(const scanner::ScanResult& result);

} // namespace commands
