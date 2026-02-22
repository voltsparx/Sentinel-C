#include "dispatcher.h"
#include "baseline_ops.h"
#include "maintenance_ops.h"
#include "prompt_console.h"
#include "scan_ops.h"
#include "../core/logger.h"

namespace commands {

ExitCode dispatch(const ParsedArgs& parsed) {
    const std::string& command = parsed.command;

    if (command == "--help" || command == "-h") {
        print_help();
        return ExitCode::Ok;
    }

    if (command == "--about") {
        if (!validate_known_options(parsed, {}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        if (!reject_positionals(parsed)) {
            return ExitCode::UsageError;
        }
        print_about();
        return ExitCode::Ok;
    }

    if (command == "--explain") {
        if (!validate_known_options(parsed, {}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        if (!reject_positionals(parsed)) {
            return ExitCode::UsageError;
        }
        print_explain();
        return ExitCode::Ok;
    }

    if (command == "--version") {
        if (!validate_known_options(parsed, {"json"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        if (!reject_positionals(parsed)) {
            return ExitCode::UsageError;
        }
        print_version(has_switch(parsed, "json"));
        return ExitCode::Ok;
    }

    if (command == "--init") {
        if (!validate_known_options(parsed, {"force", "json", "quiet", "no-advice"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_init(parsed);
    }

    if (command == "--scan") {
        if (!validate_known_options(parsed,
                                    {"json", "strict", "quiet", "no-advice", "no-reports", "hash-only"},
                                    {"report-formats", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_scan_mode(parsed, ScanMode::Scan);
    }

    if (command == "--update") {
        if (!validate_known_options(parsed,
                                    {"json", "strict", "quiet", "no-advice", "no-reports", "hash-only"},
                                    {"report-formats", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_scan_mode(parsed, ScanMode::Update);
    }

    if (command == "--status") {
        if (!validate_known_options(parsed, {"json", "quiet", "no-advice", "hash-only"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_scan_mode(parsed, ScanMode::Status);
    }

    if (command == "--verify") {
        if (!validate_known_options(parsed,
                                    {"reports", "json", "strict", "quiet", "no-advice", "hash-only"},
                                    {"report-formats", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_scan_mode(parsed, ScanMode::Verify);
    }

    if (command == "--watch") {
        if (!validate_known_options(parsed,
                                    {"reports", "fail-fast", "json", "strict", "quiet", "no-advice", "hash-only"},
                                    {"interval", "cycles", "report-formats", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_watch(parsed);
    }

    if (command == "--doctor") {
        if (!validate_known_options(parsed, {"fix", "json", "quiet", "no-advice"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_doctor(parsed);
    }

    if (command == "--set-destination") {
        if (!validate_known_options(parsed, {"json", "quiet"}, {})) {
            return ExitCode::UsageError;
        }
        return handle_set_destination(parsed);
    }

    if (command == "--show-destination") {
        if (!validate_known_options(parsed, {"json", "quiet"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_show_destination(parsed);
    }

    if (command == "--guard") {
        if (!validate_known_options(parsed, {"fix", "json", "quiet", "no-advice"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_guard(parsed);
    }

    if (command == "--list-baseline") {
        if (!validate_known_options(parsed, {"json"}, {"limit", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_list_baseline(parsed);
    }

    if (command == "--show-baseline") {
        if (!validate_known_options(parsed, {"json"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_show_baseline(parsed);
    }

    if (command == "--export-baseline") {
        if (!validate_known_options(parsed, {"overwrite"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_export_baseline(parsed);
    }

    if (command == "--import-baseline") {
        if (!validate_known_options(parsed, {"force"}, {"output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_import_baseline(parsed);
    }

    if (command == "--purge-reports") {
        if (!validate_known_options(parsed, {"all", "dry-run"}, {"days", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_purge_reports(parsed);
    }

    if (command == "--tail-log") {
        if (!validate_known_options(parsed, {}, {"lines", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_tail_log(parsed);
    }

    if (command == "--report-index") {
        if (!validate_known_options(parsed, {"json"}, {"limit", "type", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_report_index(parsed);
    }

    if (command == "--prompt-mode") {
        if (!validate_known_options(parsed,
                                    {"reports", "strict", "quiet", "no-advice", "hash-only"},
                                    {"target", "interval", "cycles", "report-formats", "output-root"})) {
            return ExitCode::UsageError;
        }
        return handle_prompt(parsed);
    }

    if (command == "--prompt") {
        logger::error("Command renamed. Use --prompt-mode.");
        return ExitCode::UsageError;
    }

    logger::error("Unknown command: " + command);
    print_help();
    return ExitCode::UsageError;
}

} // namespace commands
