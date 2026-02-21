#pragma once

#include "common.h"
#include <string>
#include <vector>

namespace commands {

std::vector<std::string> build_init_advice(std::size_t scanned_files);
std::vector<std::string> build_scan_advice(const scanner::ScanResult& result,
                                           ScanMode mode,
                                           bool baseline_refreshed);
std::vector<std::string> build_watch_advice(bool any_changes,
                                            int cycles,
                                            int interval_seconds,
                                            bool fail_fast);
std::vector<std::string> build_doctor_advice(std::size_t pass_count,
                                             std::size_t warn_count,
                                             std::size_t fail_count);

void print_advice(const std::vector<std::string>& lines);

} // namespace commands
