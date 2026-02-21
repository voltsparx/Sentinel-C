#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace commands {

struct ParsedArgs {
    std::string command;
    std::vector<std::string> positionals;
    std::unordered_map<std::string, std::string> options;
    std::unordered_set<std::string> switches;
    std::string error;
};

ParsedArgs parse_args(int argc, char* argv[]);

bool has_switch(const ParsedArgs& args, const std::string& name);
std::optional<std::string> option_value(const ParsedArgs& args, const std::string& name);

bool parse_positive_int(const std::string& text, int& out);

} // namespace commands
