#include "cli.h"
#include "commands/arg_parser.h"
#include "commands/common.h"
#include "commands/dispatcher.h"
#include "core/fsutil.h"
#include "core/logger.h"

namespace cli {

int parse(int argc, char* argv[]) {
    fsutil::ensure_dirs();
    logger::init();

    if (argc < 2) {
        commands::print_help();
        return static_cast<int>(commands::ExitCode::UsageError);
    }

    const commands::ParsedArgs parsed = commands::parse_args(argc, argv);
    if (!parsed.error.empty()) {
        logger::error(parsed.error);
        commands::print_usage_lines();
        return static_cast<int>(commands::ExitCode::UsageError);
    }

    const commands::ExitCode code = commands::dispatch(parsed);
    return static_cast<int>(code);
}

} // namespace cli
