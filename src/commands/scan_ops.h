#pragma once

#include "common.h"

namespace commands {

ExitCode load_baseline(BaselineView& baseline, bool quiet = false);
ExitCode compare_target(const std::string& target,
                        ScanOutcome& outcome,
                        bool quiet = false,
                        bool consider_mtime = true);

ExitCode handle_init(const ParsedArgs& parsed);
ExitCode handle_scan_mode(const ParsedArgs& parsed, ScanMode mode);
ExitCode handle_watch(const ParsedArgs& parsed);

} // namespace commands
