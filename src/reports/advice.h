#pragma once

#include <string>
#include <vector>
#include "../scanner/scanner.h"

namespace reports {

struct AdvisorNarrative {
    std::string summary;
    std::string risk_level;
    std::vector<std::string> whys;
    std::vector<std::string> what_matters;
    std::vector<std::string> teaching;
    std::vector<std::string> next_steps;
};

std::string advisor_status(const scanner::ScanResult& result);
AdvisorNarrative advisor_narrative(const scanner::ScanResult& result);

} // namespace reports
