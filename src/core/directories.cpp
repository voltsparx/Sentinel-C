#include "directories.h"
#include "config.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace core {

void ensure_directories() {
    fs::create_directories(config::DATA_DIR);
    fs::create_directories(config::LOG_DIR);
    fs::create_directories(config::REPORT_CLI_DIR);
    fs::create_directories(config::REPORT_HTML_DIR);
    fs::create_directories(config::REPORT_JSON_DIR);
    fs::create_directories(config::REPORT_CSV_DIR);
}

}
