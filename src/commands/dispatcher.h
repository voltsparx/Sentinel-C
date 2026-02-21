#pragma once

#include "arg_parser.h"
#include "common.h"

namespace commands {

ExitCode dispatch(const ParsedArgs& parsed);

} // namespace commands
