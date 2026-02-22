#include "prompt_console.h"
#include "arg_parser.h"
#include "dispatcher.h"
#include "../banner.h"
#include "../core/config.h"
#include "../core/fsutil.h"
#include "../core/logger.h"
#include "../core/runtime_settings.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace commands {

namespace {

constexpr const char* ANSI_RESET = "\033[0m";
constexpr const char* ANSI_CYAN = "\033[36m";
constexpr const char* ANSI_GREEN = "\033[32m";
constexpr const char* ANSI_GREY = "\033[90m";

std::atomic<bool> g_interrupted{false};

struct PromptSession {
    std::string target;
    int interval = 5;
    int cycles = 12;
    bool reports = false;
    bool strict = false;
    bool hash_only = false;
    bool quiet = false;
    bool no_advice = false;
    std::string report_formats = "all";
};

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim_copy(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string join_tail_tokens(const std::vector<std::string>& tokens, std::size_t start_index) {
    if (start_index >= tokens.size()) {
        return "";
    }

    std::string joined = tokens[start_index];
    for (std::size_t i = start_index + 1; i < tokens.size(); ++i) {
        joined += " ";
        joined += tokens[i];
    }
    return joined;
}

bool parse_on_off(const std::string& value, bool& out) {
    const std::string lowered = lower_copy(value);
    if (lowered == "on" || lowered == "true" || lowered == "1" || lowered == "yes") {
        out = true;
        return true;
    }
    if (lowered == "off" || lowered == "false" || lowered == "0" || lowered == "no") {
        out = false;
        return true;
    }
    return false;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    char quote = '\0';
    bool escape = false;

    for (const char ch : line) {
        if (quote != '\0') {
            if (escape) {
                current.push_back(ch);
                escape = false;
                continue;
            }
            if (ch == '\\') {
                escape = true;
                continue;
            }
            if (ch == quote) {
                quote = '\0';
            } else {
                current.push_back(ch);
            }
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (escape) {
        current.push_back('\\');
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string style(const std::string& text, const char* color) {
    if (!config::COLOR_OUTPUT) {
        return text;
    }
    return std::string(color) + text + ANSI_RESET;
}

void clear_screen() {
#ifdef _WIN32
    const int rc = std::system("cls");
    if (rc != 0) {
        // ANSI fallback for shells where cls is unavailable.
        std::cout << "\033[2J\033[H";
    }
#else
    const char* term = std::getenv("TERM");
    if (term != nullptr && term[0] != '\0') {
        const int rc = std::system("clear");
        if (rc != 0) {
            std::cout << "\033[2J\033[H";
        }
    } else {
        // Fallback when TERM is unavailable (non-interactive shells/pipes).
        std::cout << "\033[2J\033[H";
    }
#endif
}

void print_prompt_help() {
    std::cout
        << "Prompt Commands:\n"
        << "  help                         Show this prompt help\n"
        << "  show config                  Show current session configuration\n"
        << "  set target <path>            Set default target directory\n"
        << "  set destination <path>       Set log/report/baseline destination root\n"
        << "                               (saved for future Sentinel-C runs)\n"
        << "  set interval <n>             Set default watch interval in seconds\n"
        << "  set cycles <n>               Set default watch cycles\n"
        << "  set reports <on|off>         Enable/disable report generation for verify/watch\n"
        << "  set strict <on|off>          Return exit 2 on drift for scan/update\n"
        << "  set hash-only <on|off>       Compare by hash+size only (ignore mtime drift)\n"
        << "  set quiet <on|off>           Reduce terminal output volume\n"
        << "  set advice <on|off>          Enable/disable guidance in terminal\n"
        << "  set formats <csv|cli|html|json|all|none[,..]>\n"
        << "                               Default report formats for report-generating commands\n"
        << "  use <path>                   Shortcut for: set target <path>\n"
        << "  run <command ...>            Execute a command with session defaults\n"
        << "\n"
        << "Direct command aliases:\n"
        << "  init | scan | update | status | verify | watch | doctor | guard\n"
        << "  list-baseline | show-baseline | export-baseline | import-baseline\n"
        << "  purge-reports | tail-log | report-index | set-destination | show-destination\n"
        << "  version | about | explain | help\n"
        << "\n"
        << "Prompt-only keywords:\n"
        << "  banner                       Clear screen and print the Sentinel-C banner\n"
        << "  clear                        Clear the screen\n"
        << "  exit                         Exit prompt mode (Ctrl+C also exits)\n";
}

void print_prompt_config(const PromptSession& session) {
    std::cout
        << "Prompt Session Config\n"
        << "  target        : " << (session.target.empty() ? "(not set)" : session.target) << "\n"
        << "  output-root   : " << config::output_root() << "\n"
        << "  interval      : " << session.interval << "\n"
        << "  cycles        : " << session.cycles << "\n"
        << "  reports       : " << (session.reports ? "on" : "off") << "\n"
        << "  strict        : " << (session.strict ? "on" : "off") << "\n"
        << "  hash-only     : " << (session.hash_only ? "on" : "off") << "\n"
        << "  quiet         : " << (session.quiet ? "on" : "off") << "\n"
        << "  advice        : " << (session.no_advice ? "off" : "on") << "\n"
        << "  report-formats: " << session.report_formats << "\n";
}

std::unordered_map<std::string, std::string> alias_map() {
    return {
        {"init", "--init"},
        {"scan", "--scan"},
        {"update", "--update"},
        {"status", "--status"},
        {"verify", "--verify"},
        {"watch", "--watch"},
        {"doctor", "--doctor"},
        {"guard", "--guard"},
        {"list-baseline", "--list-baseline"},
        {"show-baseline", "--show-baseline"},
        {"export-baseline", "--export-baseline"},
        {"import-baseline", "--import-baseline"},
        {"purge-reports", "--purge-reports"},
        {"tail-log", "--tail-log"},
        {"report-index", "--report-index"},
        {"set-destination", "--set-destination"},
        {"show-destination", "--show-destination"},
        {"version", "--version"},
        {"about", "--about"},
        {"explain", "--explain"},
        {"help", "--help"}
    };
}

bool token_exists(const std::vector<std::string>& tokens, const std::string& key) {
    return std::find(tokens.begin(), tokens.end(), key) != tokens.end();
}

bool option_consumes_next(const std::string& token) {
    if (!starts_with(token, "--")) {
        return false;
    }
    if (token.find('=') != std::string::npos) {
        return false;
    }
    const std::string key = token.substr(2);
    return key == "interval" || key == "cycles" || key == "report-formats" ||
           key == "limit" || key == "lines" || key == "type" || key == "days" ||
           key == "output-root";
}

bool has_positional_token(const std::vector<std::string>& tokens) {
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];
        if (starts_with(token, "--")) {
            if (option_consumes_next(token) && i + 1 < tokens.size() &&
                !starts_with(tokens[i + 1], "--")) {
                ++i;
            }
            continue;
        }
        return true;
    }
    return false;
}

void apply_session_defaults(std::vector<std::string>& tokens, const PromptSession& session) {
    if (tokens.empty()) {
        return;
    }

    const std::string& command = tokens.front();
    const bool is_target_command =
        command == "--init" || command == "--scan" || command == "--update" ||
        command == "--status" || command == "--verify" || command == "--watch";

    const bool has_target = has_positional_token(tokens);

    if (is_target_command && !has_target && !session.target.empty()) {
        tokens.push_back(session.target);
    }

    if (command == "--watch") {
        if (!token_exists(tokens, "--interval")) {
            tokens.push_back("--interval");
            tokens.push_back(std::to_string(session.interval));
        }
        if (!token_exists(tokens, "--cycles")) {
            tokens.push_back("--cycles");
            tokens.push_back(std::to_string(session.cycles));
        }
    }

    const bool report_capable =
        command == "--scan" || command == "--update" || command == "--verify" || command == "--watch";
    if (report_capable) {
        if (!token_exists(tokens, "--report-formats") && session.report_formats != "all") {
            tokens.push_back("--report-formats");
            tokens.push_back(session.report_formats);
        }

        if ((command == "--verify" || command == "--watch") &&
            session.reports &&
            !token_exists(tokens, "--reports") &&
            !token_exists(tokens, "--report-formats")) {
            tokens.push_back("--reports");
        }
    }

    const bool toggle_capable =
        command == "--init" || command == "--scan" || command == "--update" ||
        command == "--status" || command == "--verify" || command == "--watch" ||
        command == "--doctor" || command == "--guard";
    if (toggle_capable) {
        if (session.strict && !token_exists(tokens, "--strict") &&
            (command == "--scan" || command == "--update")) {
            tokens.push_back("--strict");
        }
        if (session.hash_only && !token_exists(tokens, "--hash-only") &&
            (command == "--scan" || command == "--update" ||
             command == "--status" || command == "--verify" || command == "--watch")) {
            tokens.push_back("--hash-only");
        }
        if (session.quiet && !token_exists(tokens, "--quiet")) {
            tokens.push_back("--quiet");
        }
        if (session.no_advice && !token_exists(tokens, "--no-advice")) {
            tokens.push_back("--no-advice");
        }
    }
}

ParsedArgs parse_from_tokens(const std::vector<std::string>& tokens) {
    ParsedArgs parsed;
    if (tokens.empty()) {
        parsed.error = "No command provided.";
        return parsed;
    }

    std::vector<std::string> argv_storage;
    argv_storage.reserve(tokens.size() + 1);
    argv_storage.push_back("sentinel-c");
    for (const std::string& token : tokens) {
        argv_storage.push_back(token);
    }

    std::vector<char*> argv;
    argv.reserve(argv_storage.size());
    for (std::string& token : argv_storage) {
        argv.push_back(token.data());
    }

    return parse_args(static_cast<int>(argv.size()), argv.data());
}

void on_sigint(int) {
    g_interrupted.store(true, std::memory_order_relaxed);
}

bool handle_set(PromptSession& session, const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        logger::error("Usage: set <target|destination|interval|cycles|reports|strict|hash-only|quiet|advice|formats> <value>");
        return true;
    }

    const std::string key = lower_copy(tokens[1]);
    const std::string value = trim_copy(join_tail_tokens(tokens, 2));

    if (key == "target") {
        session.target = value;
        logger::success("Default target updated.");
        return true;
    }
    if (key == "destination" || key == "output-root" || key == "output") {
        std::string error;
        if (!config::set_output_root(value, &error)) {
            logger::error("Failed to set destination: " + error);
            return true;
        }
        fsutil::ensure_dirs();
        logger::reopen();
        if (!core::save_output_root(config::output_root(), &error)) {
            logger::error("Destination applied but not saved: " + error);
            return true;
        }
        logger::success("Output destination updated: " + config::output_root());
        logger::info("Saved to: " + core::settings_file_path());
        return true;
    }

    if (key == "interval") {
        int parsed = 0;
        if (!parse_positive_int(value, parsed)) {
            logger::error("interval must be a positive integer.");
            return true;
        }
        session.interval = parsed;
        logger::success("Default interval updated.");
        return true;
    }

    if (key == "cycles") {
        int parsed = 0;
        if (!parse_positive_int(value, parsed)) {
            logger::error("cycles must be a positive integer.");
            return true;
        }
        session.cycles = parsed;
        logger::success("Default cycles updated.");
        return true;
    }

    bool parsed_bool = false;
    if (key == "reports") {
        if (!parse_on_off(value, parsed_bool)) {
            logger::error("reports value must be on/off.");
            return true;
        }
        session.reports = parsed_bool;
        logger::success("Default reports toggle updated.");
        return true;
    }
    if (key == "strict") {
        if (!parse_on_off(value, parsed_bool)) {
            logger::error("strict value must be on/off.");
            return true;
        }
        session.strict = parsed_bool;
        logger::success("Default strict toggle updated.");
        return true;
    }
    if (key == "hash-only") {
        if (!parse_on_off(value, parsed_bool)) {
            logger::error("hash-only value must be on/off.");
            return true;
        }
        session.hash_only = parsed_bool;
        logger::success("Default hash-only toggle updated.");
        return true;
    }
    if (key == "quiet") {
        if (!parse_on_off(value, parsed_bool)) {
            logger::error("quiet value must be on/off.");
            return true;
        }
        session.quiet = parsed_bool;
        logger::success("Default quiet toggle updated.");
        return true;
    }
    if (key == "advice") {
        if (!parse_on_off(value, parsed_bool)) {
            logger::error("advice value must be on/off.");
            return true;
        }
        session.no_advice = !parsed_bool;
        logger::success("Default advice toggle updated.");
        return true;
    }
    if (key == "formats") {
        session.report_formats = lower_copy(value);
        logger::success("Default report formats updated.");
        return true;
    }

    logger::error("Unknown set key: " + key);
    return true;
}

bool run_prompt_command(PromptSession& session, const std::vector<std::string>& input_tokens) {
    if (input_tokens.empty()) {
        return true;
    }

    std::vector<std::string> tokens = input_tokens;
    const std::string first = lower_copy(tokens.front());

    if (first == "exit" || first == "quit") {
        return false;
    }
    if (first == "banner") {
        clear_screen();
        show_banner();
        return true;
    }
    if (first == "clear") {
        clear_screen();
        return true;
    }
    if (first == "help") {
        print_prompt_help();
        return true;
    }
    if (first == "show" && tokens.size() >= 2 && lower_copy(tokens[1]) == "config") {
        print_prompt_config(session);
        return true;
    }
    if (first == "set") {
        return handle_set(session, tokens);
    }
    if (first == "use") {
        if (tokens.size() < 2) {
            logger::error("Usage: use <path>");
            return true;
        }
        session.target = trim_copy(join_tail_tokens(tokens, 1));
        logger::success("Default target updated.");
        return true;
    }
    if (first == "run") {
        if (tokens.size() < 2) {
            logger::error("Usage: run <command ...>");
            return true;
        }
        tokens.erase(tokens.begin());
    }

    if (!starts_with(tokens.front(), "--")) {
        const auto aliases = alias_map();
        const auto it = aliases.find(lower_copy(tokens.front()));
        if (it != aliases.end()) {
            tokens.front() = it->second;
        }
    }

    apply_session_defaults(tokens, session);
    ParsedArgs parsed = parse_from_tokens(tokens);
    if (!parsed.error.empty()) {
        logger::error(parsed.error);
        return true;
    }

    if (parsed.command == "--prompt-mode" || parsed.command == "--prompt") {
        logger::error("Prompt mode is already active.");
        return true;
    }
    if (const auto output_root = option_value(parsed, "output-root"); output_root.has_value()) {
        std::string error;
        if (!config::set_output_root(*output_root, &error)) {
            logger::error("Failed to set output destination: " + error);
            return true;
        }
        fsutil::ensure_dirs();
        logger::reopen();
    }

    const ExitCode code = dispatch(parsed);
    std::cout << style("command exit=", ANSI_GREY) << static_cast<int>(code) << "\n";
    return true;
}

} // namespace

ExitCode handle_prompt(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    PromptSession session;
    if (const auto target = option_value(parsed, "target"); target.has_value()) {
        session.target = normalize_path(*target);
    }
    if (const auto interval = option_value(parsed, "interval"); interval.has_value()) {
        if (!parse_positive_int(*interval, session.interval)) {
            logger::error("Invalid --interval value for --prompt-mode.");
            return ExitCode::UsageError;
        }
    }
    if (const auto cycles = option_value(parsed, "cycles"); cycles.has_value()) {
        if (!parse_positive_int(*cycles, session.cycles)) {
            logger::error("Invalid --cycles value for --prompt-mode.");
            return ExitCode::UsageError;
        }
    }
    if (const auto formats = option_value(parsed, "report-formats"); formats.has_value()) {
        session.report_formats = lower_copy(*formats);
    }
    session.reports = has_switch(parsed, "reports");
    session.strict = has_switch(parsed, "strict");
    session.hash_only = has_switch(parsed, "hash-only");
    session.quiet = has_switch(parsed, "quiet");
    session.no_advice = has_switch(parsed, "no-advice");

    show_banner();
    std::cout << style("Sentinel-C Prompt Mode", ANSI_GREEN) << "\n";
    std::cout << "Type " << style("help", ANSI_CYAN)
              << " for console guidance. "
              << "Use " << style("exit", ANSI_CYAN)
              << " or Ctrl+C to leave.\n\n";
    print_prompt_config(session);
    std::cout << "\n";

    g_interrupted.store(false, std::memory_order_relaxed);
    auto previous_handler = std::signal(SIGINT, on_sigint);

    std::string line;
    while (!g_interrupted.load(std::memory_order_relaxed)) {
        std::cout << style("sentinel-c> ", ANSI_CYAN);
        if (!std::getline(std::cin, line)) {
            if (g_interrupted.load(std::memory_order_relaxed)) {
                break;
            }
            if (std::cin.eof()) {
                break;
            }
            std::cin.clear();
            continue;
        }

        const std::vector<std::string> tokens = tokenize(line);
        if (!run_prompt_command(session, tokens)) {
            break;
        }
    }

    std::signal(SIGINT, previous_handler);
    std::cout << "\n" << style("Leaving prompt mode.", ANSI_GREY) << "\n";
    return ExitCode::Ok;
}

} // namespace commands
