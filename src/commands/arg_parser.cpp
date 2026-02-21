#include "arg_parser.h"
#include <charconv>
#include <system_error>

namespace {

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool is_boolean_option(const std::string& key) {
    return key == "json" ||
           key == "force" ||
           key == "reports" ||
           key == "fail-fast" ||
           key == "fix" ||
           key == "overwrite" ||
           key == "all" ||
           key == "dry-run" ||
           key == "strict" ||
           key == "quiet" ||
           key == "no-advice" ||
           key == "no-reports" ||
           key == "hash-only";
}

} // namespace

namespace commands {

ParsedArgs parse_args(int argc, char* argv[]) {
    ParsedArgs parsed;
    if (argc < 2) {
        parsed.error = "No command provided.";
        return parsed;
    }

    parsed.command = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string token = argv[i];
        if (starts_with(token, "--")) {
            const std::size_t eq = token.find('=');
            if (eq != std::string::npos) {
                const std::string key = token.substr(2, eq - 2);
                const std::string value = token.substr(eq + 1);
                if (key.empty()) {
                    parsed.error = "Invalid option: " + token;
                    return parsed;
                }
                parsed.options[key] = value;
                continue;
            }

            const std::string key = token.substr(2);
            if (key.empty()) {
                parsed.error = "Invalid option: " + token;
                return parsed;
            }

            if (is_boolean_option(key)) {
                parsed.switches.insert(key);
                continue;
            }

            if (i + 1 < argc) {
                const std::string next = argv[i + 1];
                if (!starts_with(next, "--")) {
                    parsed.options[key] = next;
                    ++i;
                    continue;
                }
            }

            parsed.switches.insert(key);
            continue;
        }

        parsed.positionals.push_back(token);
    }

    return parsed;
}

bool has_switch(const ParsedArgs& args, const std::string& name) {
    return args.switches.find(name) != args.switches.end();
}

std::optional<std::string> option_value(const ParsedArgs& args, const std::string& name) {
    const auto it = args.options.find(name);
    if (it == args.options.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool parse_positive_int(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }

    int value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || value <= 0) {
        return false;
    }

    out = value;
    return true;
}

} // namespace commands
