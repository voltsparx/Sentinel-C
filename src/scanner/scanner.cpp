#include "scanner.h"
#include "hash.h"
#include "ignore.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::time_t to_time_t(const fs::file_time_type& file_time) {
    using namespace std::chrono;
    const auto system_now = std::chrono::system_clock::now();
    const auto file_now = fs::file_time_type::clock::now();
    const auto converted =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - file_now + system_now
        );
    return std::chrono::system_clock::to_time_t(converted);
}

std::string normalize_path(const fs::path& path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.lexically_normal().generic_string();
}

struct PendingFile {
    std::string path;
    uintmax_t size = 0;
    std::time_t mtime = 0;
};

} // namespace

namespace scanner {

FileMap build_snapshot(const std::string& target, core::ScanStats* stats) {
    if (stats != nullptr) {
        *stats = core::ScanStats{};
    }

    const auto start = std::chrono::steady_clock::now();
    ignore::load();

    FileMap current;
    std::vector<PendingFile> pending;
    pending.reserve(4096);
    const fs::path root_path(target);
    std::error_code ec;
    const auto options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(target, options, ec);
    fs::recursive_directory_iterator end;

    while (it != end) {
        if (ec) {
            ec.clear();
            it.increment(ec);
            continue;
        }

        const fs::directory_entry entry = *it;
        it.increment(ec);

        if (!entry.is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const std::string path = normalize_path(entry.path());
        std::error_code rel_ec;
        const fs::path rel = fs::relative(entry.path(), root_path, rel_ec);
        const std::string relative_path =
            rel_ec ? entry.path().filename().generic_string() : rel.generic_string();

        if (ignore::match(path) || ignore::match(relative_path)) {
            continue;
        }

        const uintmax_t size = entry.file_size(ec);
        if (ec) {
            ec.clear();
            continue;
        }

        const fs::file_time_type last_write = entry.last_write_time(ec);
        if (ec) {
            ec.clear();
            continue;
        }
        pending.push_back(PendingFile{path, size, to_time_t(last_write)});
    }

    if (!pending.empty()) {
        current.reserve(pending.size());

        const unsigned int hw = std::max(1u, std::thread::hardware_concurrency());
        const std::size_t workers = std::min<std::size_t>(pending.size(), hw);

        if (workers <= 1 || pending.size() < 64) {
            for (const PendingFile& item : pending) {
                core::FileEntry entry;
                entry.path = item.path;
                entry.size = item.size;
                entry.mtime = item.mtime;
                entry.hash = hash::sha256_file(item.path);
                if (entry.hash.empty()) {
                    continue;
                }
                current.emplace(entry.path, std::move(entry));
            }
        } else {
            std::atomic<std::size_t> next_index{0};
            std::mutex map_lock;
            std::vector<std::thread> pool;
            pool.reserve(workers);

            for (std::size_t worker = 0; worker < workers; ++worker) {
                pool.emplace_back([&]() {
                    std::vector<core::FileEntry> local_entries;
                    local_entries.reserve(64);

                    while (true) {
                        const std::size_t index =
                            next_index.fetch_add(1, std::memory_order_relaxed);
                        if (index >= pending.size()) {
                            break;
                        }

                        const PendingFile& item = pending[index];
                        const std::string digest = hash::sha256_file(item.path);
                        if (digest.empty()) {
                            continue;
                        }

                        core::FileEntry entry;
                        entry.path = item.path;
                        entry.size = item.size;
                        entry.mtime = item.mtime;
                        entry.hash = digest;
                        local_entries.push_back(std::move(entry));
                    }

                    if (!local_entries.empty()) {
                        std::lock_guard<std::mutex> guard(map_lock);
                        for (core::FileEntry& entry : local_entries) {
                            current.emplace(entry.path, std::move(entry));
                        }
                    }
                });
            }

            for (std::thread& worker : pool) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }
    }

    if (stats != nullptr) {
        stats->scanned = current.size();
    }

    if (stats != nullptr) {
        stats->duration =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                .count();
    }

    return current;
}

ScanResult compare(const FileMap& baseline, const FileMap& current, bool consider_mtime) {
    ScanResult result;
    result.current = current;
    result.stats.scanned = current.size();

    for (const auto& item : current) {
        const std::string& path = item.first;
        const core::FileEntry& entry = item.second;

        const auto baseline_it = baseline.find(path);
        if (baseline_it == baseline.end()) {
            result.added[path] = entry;
            continue;
        }

        const core::FileEntry& old = baseline_it->second;
        const bool mtime_changed =
            consider_mtime && (old.mtime != 0 && entry.mtime != 0 && old.mtime != entry.mtime);
        if (old.hash != entry.hash || old.size != entry.size || mtime_changed) {
            result.modified[path] = entry;
        }
    }

    for (const auto& item : baseline) {
        const std::string& path = item.first;
        const core::FileEntry& entry = item.second;
        if (current.find(path) == current.end()) {
            result.deleted[path] = entry;
        }
    }

    result.stats.added = result.added.size();
    result.stats.modified = result.modified.size();
    result.stats.deleted = result.deleted.size();
    return result;
}

ScanResult compare(const FileMap& baseline, const FileMap& current) {
    return compare(baseline, current, true);
}

} // namespace scanner
