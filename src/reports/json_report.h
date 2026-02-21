#pragma once
#include <string>
#include "../scanner/scanner.h"

namespace reports {
std::string write_json(const scanner::ScanResult& result, const std::string& scan_id);
}
