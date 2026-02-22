#include "ignore.h"
#include "../core/config.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> rules;

std::string normalize(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
#ifdef _WIN32
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return text;
}

std::string trim(const std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool wildcard_match(const std::string& text, const std::string& pattern) {
    if (pattern.find('*') == std::string::npos) {
        return text.find(pattern) != std::string::npos;
    }

    std::size_t text_pos = 0;
    std::size_t pattern_pos = 0;
    bool first = true;
    std::string last_token;

    while (pattern_pos < pattern.size()) {
        const std::size_t star = pattern.find('*', pattern_pos);
        const std::string token = (star == std::string::npos)
                                      ? pattern.substr(pattern_pos)
                                      : pattern.substr(pattern_pos, star - pattern_pos);

        if (!token.empty()) {
            const std::size_t found = text.find(token, text_pos);
            if (found == std::string::npos) {
                return false;
            }
            if (first && pattern[0] != '*' && found != 0) {
                return false;
            }
            text_pos = found + token.size();
            last_token = token;
            first = false;
        }

        if (star == std::string::npos) {
            break;
        }
        pattern_pos = star + 1;
    }

    if (!pattern.empty() && pattern.back() != '*' && !last_token.empty()) {
        return ends_with(text, last_token);
    }
    return true;
}

bool load_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        rules.push_back(normalize(line));
    }
    return true;
}

} // namespace

namespace ignore {

void load() {
    rules.clear();

    rules.push_back("sentinel-c-logs/");
    rules.push_back("sentinel-c-logs\\");

    if (!load_from_file(config::IGNORE_FILE)) {
        load_from_file(config::PROJECT_ROOT + "/src/.sentinelignore");
    }
}

bool match(const std::string& path) {
    const std::string normalized = normalize(path);
    for (const std::string& rule : rules) {
        if (wildcard_match(normalized, rule)) {
            return true;
        }
    }
    return false;
}

} // namespace ignore
