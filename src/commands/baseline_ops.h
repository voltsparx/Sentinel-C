#pragma once

#include "common.h"

namespace commands {

ExitCode handle_list_baseline(const ParsedArgs& parsed);
ExitCode handle_show_baseline(const ParsedArgs& parsed);
ExitCode handle_export_baseline(const ParsedArgs& parsed);
ExitCode handle_import_baseline(const ParsedArgs& parsed);

} // namespace commands
