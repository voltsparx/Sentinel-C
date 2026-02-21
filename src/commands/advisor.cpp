#include "advisor.h"
#include "../core/config.h"
#include <iostream>

namespace commands {

namespace {

constexpr const char* ANSI_RESET = "\033[0m";
constexpr const char* ANSI_CYAN = "\033[36m";
constexpr const char* ANSI_GREY = "\033[90m";

std::string styled(const std::string& text, const char* color) {
    if (!config::COLOR_OUTPUT) {
        return text;
    }
    return std::string(color) + text + ANSI_RESET;
}

} // namespace

std::vector<std::string> build_init_advice(std::size_t scanned_files) {
    std::vector<std::string> advice;
    if (scanned_files == 0) {
        advice.push_back("What happened: baseline was created but no files were tracked.");
        advice.push_back("Why this matters: without tracked files, integrity drift cannot be detected.");
        advice.push_back("What matters now: please verify target path and ignore rules.");
        advice.push_back("Teaching tip: run --list-baseline to confirm expected entries are present.");
    } else {
        advice.push_back("What happened: baseline recorded with " + std::to_string(scanned_files) + " file(s).");
        advice.push_back("Why this matters: this snapshot becomes your trust reference for future comparisons.");
        advice.push_back("What matters now: keep this baseline only if system state is known-good.");
        advice.push_back("Teaching tip: run --status regularly for lightweight integrity checks.");
    }
    return advice;
}

std::vector<std::string> build_scan_advice(const scanner::ScanResult& result,
                                           ScanMode mode,
                                           bool baseline_refreshed) {
    std::vector<std::string> advice;
    if (!has_changes(result)) {
        advice.push_back("What happened: no integrity drift was detected in this cycle.");
        advice.push_back("Why this matters: current files match your trusted baseline signal.");
        advice.push_back("What matters now: keep cadence and continue routine monitoring.");
        if (mode == ScanMode::Status || mode == ScanMode::Verify) {
            advice.push_back("Teaching tip: clean status can be used as a CI gate for integrity confidence.");
        }
        return advice;
    }

    advice.push_back("What happened: integrity drift was detected.");
    if (result.stats.added > 0) {
        advice.push_back("Why this matters: new files can indicate legitimate deployment or unauthorized dropper activity.");
        advice.push_back("What matters now: review " + std::to_string(result.stats.added) +
                         " added file(s), especially scripts, binaries, and startup paths.");
    }
    if (result.stats.modified > 0) {
        advice.push_back("Why this matters: modified files can alter runtime behavior and trust assumptions.");
        advice.push_back("What matters now: verify " + std::to_string(result.stats.modified) +
                         " modified file(s) against approved patch/change records.");
    }
    if (result.stats.deleted > 0) {
        advice.push_back("Why this matters: unexpected deletions can hide traces or break controls.");
        advice.push_back("What matters now: confirm " + std::to_string(result.stats.deleted) +
                         " deleted file(s) were intentional.");
    }

    if (mode == ScanMode::Status) {
        advice.push_back("Teaching tip: status mode is for fast automation; use reports for deeper triage.");
    }
    if (mode == ScanMode::Verify) {
        advice.push_back("Teaching tip: verify mode is best before baseline refresh in controlled rollouts.");
    }
    if (baseline_refreshed) {
        advice.push_back("What matters now: baseline was refreshed, so please keep approval evidence and change context.");
    } else {
        advice.push_back("What matters now: if these changes are approved, run --update to align baseline.");
    }
    return advice;
}

std::vector<std::string> build_watch_advice(bool any_changes,
                                            int cycles,
                                            int interval_seconds,
                                            bool fail_fast) {
    std::vector<std::string> advice;
    if (!any_changes) {
        advice.push_back("What happened: watch cycles completed without detected drift.");
        advice.push_back("Why this matters: repeated clean checks increase confidence in file-state stability.");
    } else {
        advice.push_back("What happened: watch mode detected integrity drift.");
        advice.push_back("Why this matters: drift during watch suggests active file-state changes on host.");
    }

    advice.push_back("What matters now: profile used " + std::to_string(cycles) +
                     " cycle(s) at " + std::to_string(interval_seconds) + " second interval.");
    if (fail_fast) {
        advice.push_back("Teaching tip: fail-fast stops early on first alert, useful for strict CI/CD gates.");
    } else {
        advice.push_back("Teaching tip: tune interval/cycles for your change velocity and risk level.");
    }
    return advice;
}

std::vector<std::string> build_doctor_advice(std::size_t pass_count,
                                             std::size_t warn_count,
                                             std::size_t fail_count) {
    std::vector<std::string> advice;
    if (fail_count == 0 && warn_count == 0) {
        advice.push_back("What happened: all environment checks passed.");
        advice.push_back("Why this matters: healthy storage and logging paths reduce monitoring blind spots.");
    } else if (fail_count == 0) {
        advice.push_back("What happened: no hard failures, but warnings were detected.");
        advice.push_back("Why this matters: unresolved warnings can become future reliability issues.");
    } else {
        advice.push_back("What happened: one or more critical health checks failed.");
        advice.push_back("Why this matters: scan results may be incomplete until failures are resolved.");
    }

    advice.push_back("What matters now: doctor summary is " + std::to_string(pass_count) + " pass, " +
                     std::to_string(warn_count) + " warn, " + std::to_string(fail_count) + " fail.");
    advice.push_back("Teaching tip: run --doctor after tool upgrades, path changes, or permission updates.");
    return advice;
}

void print_advice(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return;
    }

    std::cout << "\n" << styled("Nano Advisor", ANSI_CYAN) << "\n";
    for (const std::string& line : lines) {
        std::cout << styled("  > ", ANSI_GREY) << line << "\n";
    }
}

} // namespace commands
