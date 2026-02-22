#pragma once

#include <optional>
#include <string>

namespace core {

std::optional<std::string> load_saved_output_root(std::string* error = nullptr);
bool save_output_root(const std::string& output_root, std::string* error = nullptr);
std::string settings_file_path();

} // namespace core

