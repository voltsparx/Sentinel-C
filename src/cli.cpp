#include "cli.h"
#include "commands/arg_parser.h"
#include "commands/common.h"
#include "commands/dispatcher.h"
#include "core/config.h"
#include "core/fsutil.h"
#include "core/logger.h"
#include <iostream>

namespace cli {

int parse(int argc, char* argv[]) {
    if (argc < 2) {
        commands::print_no_command_hint();
        return static_cast<int>(commands::ExitCode::UsageError);
    }

    const commands::ParsedArgs parsed = commands::parse_args(argc, argv);
    if (!parsed.error.empty()) {
        std::cerr << "[ERROR] " << parsed.error << "\n";
        commands::print_usage_lines();
        return static_cast<int>(commands::ExitCode::UsageError);
    }

    if (const auto output_root = commands::option_value(parsed, "output-root");
        output_root.has_value()) {
        std::string error;
        if (!config::set_output_root(*output_root, &error)) {
            std::cerr << "[ERROR] Failed to set --output-root: " << error << "\n";
            return static_cast<int>(commands::ExitCode::UsageError);
        }
    }

    fsutil::ensure_dirs();
    logger::init();

    const commands::ExitCode code = commands::dispatch(parsed);
    return static_cast<int>(code);
}

} // namespace cli
