#pragma once
#include <string>

namespace logger {

enum class Level {
    INFO,
    SUCCESS,
    WARNING,
    ERROR
};

void init();
void reopen();
void write(Level level, const std::string& message);
void info(const std::string& message);
void success(const std::string& message);
void warning(const std::string& message);
void error(const std::string& message);

}
