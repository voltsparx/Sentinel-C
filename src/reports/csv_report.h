#pragma once

#include "../scanner/scanner.h"
#include <string>

namespace reports {

std::string write_csv(const scanner::ScanResult& result, const std::string& scan_id = "");

} // namespace reports

