#pragma once
#include <cstdint>
#include <string>

namespace hash {
std::string sha256_file(const std::string& path);
std::string sha256_file(const std::string& path, uintmax_t expected_size);
}
