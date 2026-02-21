#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstdint>

namespace common {

enum class LogLevel { INFO, ALERT, ERROR, SUCCESS };

struct FileEntry {
    std::string path;
    std::string hash;
    uintmax_t size;
    std::time_t mtime;
};

inline std::string now_string() {
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

inline void log(const std::string& msg, LogLevel lvl = LogLevel::INFO) {
    const char* color = "\033[0m";
    const char* tag   = "[*]";

    switch (lvl) {
        case LogLevel::SUCCESS: color = "\033[32m"; tag = "[+]"; break;
        case LogLevel::ALERT:   color = "\033[33m"; tag = "[!]"; break;
        case LogLevel::ERROR:   color = "\033[31m"; tag = "[-]"; break;
        default: break;
    }

    std::cout << color << tag << " " << msg << "\033[0m\n";
}

}
