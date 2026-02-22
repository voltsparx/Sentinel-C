#include "hasher.h"
#include "scanner/hash.h"

namespace hasher {

std::string sha256_file(const std::string& filepath) {
    // Compatibility shim for legacy modules.
    return hash::sha256_file(filepath);
}

} // namespace hasher

