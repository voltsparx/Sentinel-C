#include "hash.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>

namespace hash {

namespace {

constexpr const char* EMPTY_FILE_SHA256 =
    "e3b0c44298fc1c149afbf4c8996fb924"
    "27ae41e4649b934ca495991b7852b855";

constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2
};

inline std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

struct Sha256Context {
    std::uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    std::array<std::uint8_t, 64> buffer{};
    std::size_t buffer_len = 0;
    std::uint64_t total_bytes = 0;
};

void process_block(const std::uint8_t* block, std::uint32_t* state) {
    std::uint32_t w[64] = {};
    for (int i = 0; i < 16; ++i) {
        const int offset = i * 4;
        w[i] = (static_cast<std::uint32_t>(block[offset]) << 24) |
               (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
               (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
               static_cast<std::uint32_t>(block[offset + 3]);
    }

    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 =
            rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 =
            rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 = h + s1 + ch + K[i] + w[i];

        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void update(Sha256Context& ctx, const std::uint8_t* data, std::size_t len) {
    if (len == 0) {
        return;
    }

    ctx.total_bytes += static_cast<std::uint64_t>(len);

    std::size_t offset = 0;
    if (ctx.buffer_len > 0) {
        const std::size_t copy_len = std::min<std::size_t>(64 - ctx.buffer_len, len);
        std::memcpy(ctx.buffer.data() + ctx.buffer_len, data, copy_len);
        ctx.buffer_len += copy_len;
        offset += copy_len;

        if (ctx.buffer_len == 64) {
            process_block(ctx.buffer.data(), ctx.state);
            ctx.buffer_len = 0;
        }
    }

    while (offset + 64 <= len) {
        process_block(data + offset, ctx.state);
        offset += 64;
    }

    const std::size_t tail = len - offset;
    if (tail > 0) {
        std::memcpy(ctx.buffer.data(), data + offset, tail);
        ctx.buffer_len = tail;
    }
}

std::string finalize(Sha256Context& ctx) {
    const std::uint64_t bit_len = ctx.total_bytes * 8;

    ctx.buffer[ctx.buffer_len++] = 0x80;

    if (ctx.buffer_len > 56) {
        while (ctx.buffer_len < 64) {
            ctx.buffer[ctx.buffer_len++] = 0x00;
        }
        process_block(ctx.buffer.data(), ctx.state);
        ctx.buffer_len = 0;
    }

    while (ctx.buffer_len < 56) {
        ctx.buffer[ctx.buffer_len++] = 0x00;
    }

    for (int i = 7; i >= 0; --i) {
        ctx.buffer[ctx.buffer_len++] =
            static_cast<std::uint8_t>((bit_len >> (static_cast<std::uint64_t>(i) * 8)) & 0xFF);
    }
    process_block(ctx.buffer.data(), ctx.state);

    std::ostringstream out;
    for (const std::uint32_t value : ctx.state) {
        out << std::hex << std::setw(8) << std::setfill('0') << value;
    }
    return out.str();
}

std::string sha256_stream(std::ifstream& file,
                          const std::optional<uintmax_t>& expected_size) {
    if (expected_size.has_value() && *expected_size == 0) {
        return EMPTY_FILE_SHA256;
    }

    Sha256Context ctx;
    std::array<char, 64 * 1024> chunk{};
    std::uintmax_t remaining =
        expected_size.value_or(std::numeric_limits<std::uintmax_t>::max());

    while (file) {
        if (expected_size.has_value() && remaining == 0) {
            break;
        }

        const std::size_t request_size = expected_size.has_value()
            ? static_cast<std::size_t>(
                  std::min<std::uintmax_t>(chunk.size(), remaining))
            : chunk.size();

        file.read(chunk.data(), static_cast<std::streamsize>(request_size));
        const std::streamsize read_bytes = file.gcount();
        if (read_bytes <= 0) {
            break;
        }

        update(ctx,
               reinterpret_cast<const std::uint8_t*>(chunk.data()),
               static_cast<std::size_t>(read_bytes));

        if (expected_size.has_value()) {
            remaining -= static_cast<std::uintmax_t>(read_bytes);
        }
    }

    if (expected_size.has_value() && remaining != 0) {
        return "";
    }
    if (file.bad()) {
        return "";
    }
    return finalize(ctx);
}

std::optional<uintmax_t> detect_expected_size(const std::string& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return size;
}

} // namespace

std::string sha256_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    return sha256_stream(file, detect_expected_size(path));
}

std::string sha256_file(const std::string& path, uintmax_t expected_size) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    return sha256_stream(file, expected_size);
}

} // namespace hash
