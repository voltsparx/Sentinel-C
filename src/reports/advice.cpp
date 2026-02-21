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
        narrative.summary = "No integrity drift detected in this snapshot.";
        narrative.risk_level = "low";
        narrative.whys.push_back("Current hashes and metadata align with your trusted baseline.");
        narrative.what_matters.push_back("Stable state means your baseline remains reliable for this cycle.");
        narrative.teaching.push_back("Please continue periodic scans to maintain confidence over time.");
        narrative.teaching.push_back("A clean scan is one signal; keep patch and access reviews in place.");
        narrative.next_steps.push_back("Keep scheduled status checks in CI or task automation.");
        narrative.next_steps.push_back("Re-run doctor after environment, permission, or storage changes.");
        return narrative;
    }

    narrative.summary = "Integrity drift detected. Please review these changes before trusting the new state.";
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
            " new file(s) appeared. New binaries/scripts can be expected deployments or unauthorized drops."
        );
        narrative.what_matters.push_back(
            "Validate added files by source, signer, owner, and expected deployment record."
        );
    }
    if (result.stats.modified > 0) {
        narrative.whys.push_back(
            std::to_string(result.stats.modified) +
            " file(s) changed. Modifications can alter runtime behavior and trust assumptions."
        );
        narrative.what_matters.push_back(
            "Cross-check modified files against approved patches or maintenance windows."
        );
    }
    if (result.stats.deleted > 0) {
        narrative.whys.push_back(
            std::to_string(result.stats.deleted) +
            " file(s) were removed. Unexpected deletion can hide traces or disable controls."
        );
        narrative.what_matters.push_back(
            "Confirm deletions were intentional and documented by authorized operators."
        );
    }

    narrative.teaching.push_back("Start triage with least expected paths first, then validate known deployment paths.");
    narrative.teaching.push_back("If every change is approved, run --update to align baseline with the new trusted state.");
    narrative.teaching.push_back("If uncertain, keep current baseline and investigate before accepting drift.");
    narrative.next_steps.push_back("Check change tickets, deployment logs, and operator approvals for changed paths.");
    narrative.next_steps.push_back("Prioritize startup paths, executable files, and security-sensitive directories.");
    narrative.next_steps.push_back("Escalate immediately if drift is unexpected and cannot be explained quickly.");
    return narrative;
}

} // namespace reports
