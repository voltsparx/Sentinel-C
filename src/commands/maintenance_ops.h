#pragma once

#include "common.h"

namespace commands {

ExitCode handle_purge_reports(const ParsedArgs& parsed);
ExitCode handle_tail_log(const ParsedArgs& parsed);
ExitCode handle_doctor(const ParsedArgs& parsed);
ExitCode handle_report_index(const ParsedArgs& parsed);
ExitCode handle_guard(const ParsedArgs& parsed);
ExitCode handle_set_destination(const ParsedArgs& parsed);
ExitCode handle_show_destination(const ParsedArgs& parsed);

} // namespace commands
