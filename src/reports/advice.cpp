#include "advice.h"

namespace reports {

namespace {

bool has_changes(const scanner::ScanResult& result) {
    return result.stats.added > 0 || result.stats.modified > 0 || result.stats.deleted > 0;
}

} // namespace

std::string advisor_status(const scanner::ScanResult& result) {
    return has_changes(result) ? "changes_detected" : "clean";
}

AdvisorNarrative advisor_narrative(const scanner::ScanResult& result) {
    AdvisorNarrative narrative;
    if (!has_changes(result)) {
        narrative.summary = "No integrity drift was detected in this snapshot.";
        narrative.risk_level = "low";
        narrative.whys.push_back("Current hashes and metadata match your trusted baseline.");
        narrative.what_matters.push_back("The system state appears stable for this cycle.");
        narrative.teaching.push_back("Please continue periodic scans to maintain confidence over time.");
        narrative.teaching.push_back("Please keep patch and access reviews in place alongside clean scans.");
        narrative.next_steps.push_back("Keep scheduled status checks in CI or task automation.");
        narrative.next_steps.push_back("Please re-run doctor after environment, permission, or storage changes.");
        return narrative;
    }

    narrative.summary = "Integrity drift was detected, so please review these changes before trusting the new state.";
    const std::size_t total_changes =
        result.stats.added + result.stats.modified + result.stats.deleted;
    if (result.stats.deleted > 0 || result.stats.modified >= 5 || total_changes >= 10) {
        narrative.risk_level = "high";
    } else {
        narrative.risk_level = "medium";
    }

    if (result.stats.added > 0) {
        narrative.whys.push_back(
            std::to_string(result.stats.added) +
            " new file(s) were found, so please confirm they were expected."
        );
        narrative.what_matters.push_back(
            "Please validate new files by source, signer, owner, and deployment record."
        );
    }
    if (result.stats.modified > 0) {
        narrative.whys.push_back(
            std::to_string(result.stats.modified) +
            " file(s) were modified, so please verify them against approved changes."
        );
        narrative.what_matters.push_back(
            "Please cross-check modified files against approved patches or maintenance windows."
        );
    }
    if (result.stats.deleted > 0) {
        narrative.whys.push_back(
            std::to_string(result.stats.deleted) +
            " file(s) were deleted, so please confirm the removals were intentional."
        );
        narrative.what_matters.push_back(
            "Please confirm deletions were intentional and documented by authorized operators."
        );
    }

    narrative.teaching.push_back("Please start triage with least expected paths, then validate known deployment paths.");
    narrative.teaching.push_back("If all changes are approved, please run --update to align the baseline.");
    narrative.teaching.push_back("If anything is uncertain, keep the current baseline and investigate first.");
    narrative.next_steps.push_back("Please review change tickets, deployment logs, and operator approvals for changed paths.");
    narrative.next_steps.push_back("Please prioritize startup paths, executable files, and security-sensitive directories.");
    narrative.next_steps.push_back("Please escalate quickly if the drift is unexpected and cannot be explained.");
    return narrative;
}

} // namespace reports
