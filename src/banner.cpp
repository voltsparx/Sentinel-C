#include "banner.h"
#include "core/config.h"
#include "core/metadata.h"
#include <iostream>

void show_banner() {
    std::cout
        << "\033[36m"
        << "   ____         __  _          __    _____\n"
        << "  / __/__ ___  / /_(_)__  ___ / /___/ ___/\n"
        << " _\\ \\/ -_) _ \\/ __/ / _ \\/ -_) /___/ /__  \n"
        << "/___/\\__/_//_/\\__/_/_//_/\\__/_/    \\___/  \n"
        << "\033[0m\n"
        << "\033[90m--------------------------------------------------\033[0m\n"
        << "\033[32m" << config::TOOL_NAME << " " << config::VERSION << "\033[0m  |  "
        << "\033[33mAdvanced Host Defense Multi-Tool\033[0m\n"
        << "\033[90mCodename:\033[0m \033[35m" << config::CODENAME << "\033[0m\n"
        << "\033[90mBy:\033[0m \033[38;5;208m" << metadata::AUTHOR << "\033[0m"
        << "    |  \033[90mContact:\033[0m \033[90m" << metadata::CONTACT << "\033[0m\n"
        << "\033[90m--------------------------------------------------\033[0m\n\n";
}
