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
        advice.push_back("The baseline was created, but no files were tracked.");
        advice.push_back("Please verify the target path and ignore rules before your next scan.");
        advice.push_back("You can run --list-baseline to confirm expected entries are present.");
    } else {
        advice.push_back("The baseline was recorded with " + std::to_string(scanned_files) + " file(s).");
        advice.push_back("This snapshot is now your trusted reference for future checks.");
        advice.push_back("Please keep this baseline only if the current system state is known-good.");
        advice.push_back("You can run --status regularly for lightweight integrity checks.");
    }
    return advice;
}

std::vector<std::string> build_scan_advice(const scanner::ScanResult& result,
                                           ScanMode mode,
                                           bool baseline_refreshed) {
    std::vector<std::string> advice;
    if (!has_changes(result)) {
        advice.push_back("No integrity drift was detected in this cycle.");
        advice.push_back("The current files match your trusted baseline.");
        advice.push_back("Please continue routine monitoring at your normal cadence.");
        if (mode == ScanMode::Status || mode == ScanMode::Verify) {
            advice.push_back("This clean result can be used as a confidence signal in CI workflows.");
        }
        return advice;
    }

    advice.push_back("Integrity drift was detected and should be reviewed.");
    if (result.stats.added > 0) {
        advice.push_back(std::to_string(result.stats.added) +
                         " new file(s) were found, so please confirm they were expected.");
    }
    if (result.stats.modified > 0) {
        advice.push_back(std::to_string(result.stats.modified) +
                         " file(s) were modified, so please verify them against approved changes.");
    }
    if (result.stats.deleted > 0) {
        advice.push_back(std::to_string(result.stats.deleted) +
                         " file(s) were deleted, so please confirm the deletions were intentional.");
    }

    if (mode == ScanMode::Status) {
        advice.push_back("Status mode is optimized for quick automation checks.");
    }
    if (mode == ScanMode::Verify) {
        advice.push_back("Verify mode is useful before a baseline refresh in controlled rollouts.");
    }
    if (baseline_refreshed) {
        advice.push_back("The baseline was refreshed, so please keep your change approval records.");
    } else {
        advice.push_back("If these changes are approved, please run --update to align the baseline.");
    }
    return advice;
}

std::vector<std::string> build_watch_advice(bool any_changes,
                                            int cycles,
                                            int interval_seconds,
                                            bool fail_fast) {
    std::vector<std::string> advice;
    if (!any_changes) {
        advice.push_back("Watch mode completed without detecting integrity drift.");
        advice.push_back("Repeated clean checks increase confidence in file-state stability.");
    } else {
        advice.push_back("Watch mode detected integrity drift during monitoring.");
        advice.push_back("This suggests active file-state changes occurred on the host.");
    }

    advice.push_back("This run used " + std::to_string(cycles) +
                     " cycle(s) at a " + std::to_string(interval_seconds) + "-second interval.");
    if (fail_fast) {
        advice.push_back("Fail-fast stopped at the first alert, which is useful for strict CI/CD gates.");
    } else {
        advice.push_back("You can tune interval and cycles to match your change velocity and risk profile.");
    }
    return advice;
}

std::vector<std::string> build_doctor_advice(std::size_t pass_count,
                                             std::size_t warn_count,
                                             std::size_t fail_count) {
    std::vector<std::string> advice;
    if (fail_count == 0 && warn_count == 0) {
        advice.push_back("All environment checks passed.");
        advice.push_back("Healthy storage and logging paths reduce monitoring blind spots.");
    } else if (fail_count == 0) {
        advice.push_back("No hard failures were found, but warnings were detected.");
        advice.push_back("Please review warnings early so they do not become reliability issues.");
    } else {
        advice.push_back("One or more critical health checks failed.");
        advice.push_back("Scan results may be incomplete until these failures are resolved.");
    }

    advice.push_back("Doctor summary: " + std::to_string(pass_count) + " pass, " +
                     std::to_string(warn_count) + " warn, " + std::to_string(fail_count) + " fail.");
    advice.push_back("Please run --doctor after upgrades, path changes, or permission updates.");
    return advice;
}

void print_advice(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return;
    }

    std::cout << "\n" << styled("Guidance", ANSI_CYAN) << "\n";
    for (const std::string& line : lines) {
        std::cout << styled("  > ", ANSI_GREY) << line << "\n";
    }
}

} // namespace commands
