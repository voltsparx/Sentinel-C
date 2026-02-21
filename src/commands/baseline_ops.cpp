#include "baseline_ops.h"
#include "scan_ops.h"
#include "../core/config.h"
#include "../core/logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace commands {

ExitCode handle_list_baseline(const ParsedArgs& parsed) {
    if (!reject_positionals(parsed)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    BaselineView baseline;
    const ExitCode load_code = load_baseline(baseline, as_json);
    if (load_code != ExitCode::Ok) {
        if (as_json) {
            std::cout << "{\n"
                      << "  \"command\": \"list-baseline\",\n"
                      << "  \"exit_code\": " << static_cast<int>(load_code) << "\n"
                      << "}\n";
        }
        return load_code;
    }

    int limit = 50;
    if (!parse_positive_option(parsed, "limit", 50, limit)) {
        return ExitCode::UsageError;
    }

    std::vector<const core::FileEntry*> entries;
    entries.reserve(baseline.files.size());
    for (const auto& item : baseline.files) {
        entries.push_back(&item.second);
    }
    std::sort(entries.begin(), entries.end(),
              [](const core::FileEntry* left, const core::FileEntry* right) {
                  return left->path < right->path;
              });

    if (as_json) {
        std::cout << "{\n"
                  << "  \"root\": \"" << json_escape(baseline.root) << "\",\n"
                  << "  \"total\": " << entries.size() << ",\n"
                  << "  \"items\": [\n";
        const std::size_t count =
            std::min<std::size_t>(entries.size(), static_cast<std::size_t>(limit));
        for (std::size_t i = 0; i < count; ++i) {
            std::cout << "    {\n"
                      << "      \"path\": \"" << json_escape(entries[i]->path) << "\",\n"
                      << "      \"size\": " << entries[i]->size << ",\n"
                      << "      \"mtime\": " << entries[i]->mtime << "\n"
                      << "    }";
            if (i + 1 < count) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  ]\n}\n";
        return ExitCode::Ok;
    }

    std::cout << "Baseline Root: " << baseline.root << "\n"
              << "Tracked Files: " << entries.size() << "\n"
              << "Showing up to: " << limit << "\n\n";

    const std::size_t count =
        std::min<std::size_t>(entries.size(), static_cast<std::size_t>(limit));
    for (std::size_t i = 0; i < count; ++i) {
        std::cout << std::setw(4) << (i + 1) << "  "
                  << entries[i]->path << "  (" << entries[i]->size << " bytes)\n";
    }
    return ExitCode::Ok;
}

ExitCode handle_show_baseline(const ParsedArgs& parsed) {
    std::string query_path;
    if (!require_single_positional(parsed, "<path>", query_path)) {
        return ExitCode::UsageError;
    }

    const bool as_json = has_switch(parsed, "json");
    BaselineView baseline;
    const ExitCode load_code = load_baseline(baseline, as_json);
    if (load_code != ExitCode::Ok) {
        if (as_json) {
            std::cout << "{\n"
                      << "  \"command\": \"show-baseline\",\n"
                      << "  \"query\": \"" << json_escape(query_path) << "\",\n"
                      << "  \"exit_code\": " << static_cast<int>(load_code) << "\n"
                      << "}\n";
        }
        return load_code;
    }

    const std::string normalized_query = normalize_path(query_path);
    auto it = baseline.files.find(normalized_query);

    if (it == baseline.files.end()) {
        std::vector<const core::FileEntry*> matches;
        for (const auto& item : baseline.files) {
            if (item.first.find(query_path) != std::string::npos) {
                matches.push_back(&item.second);
            }
        }

        if (matches.empty()) {
            if (as_json) {
                std::cout << "{\n"
                          << "  \"command\": \"show-baseline\",\n"
                          << "  \"query\": \"" << json_escape(query_path) << "\",\n"
                          << "  \"exit_code\": "
                          << static_cast<int>(ExitCode::OperationFailed) << ",\n"
                          << "  \"error\": \"entry_not_found\"\n"
                          << "}\n";
            } else {
                logger::error("No baseline entry found for: " + query_path);
            }
            return ExitCode::OperationFailed;
        }

        if (matches.size() > 1) {
            if (as_json) {
                std::cout << "{\n"
                          << "  \"command\": \"show-baseline\",\n"
                          << "  \"query\": \"" << json_escape(query_path) << "\",\n"
                          << "  \"exit_code\": " << static_cast<int>(ExitCode::UsageError)
                          << ",\n"
                          << "  \"error\": \"multiple_matches\",\n"
                          << "  \"matches\": [\n";
                const std::size_t max_print =
                    std::min<std::size_t>(matches.size(), 10);
                for (std::size_t i = 0; i < max_print; ++i) {
                    std::cout << "    \"" << json_escape(matches[i]->path) << "\"";
                    if (i + 1 < max_print) {
                        std::cout << ",";
                    }
                    std::cout << "\n";
                }
                std::cout << "  ]\n}\n";
            } else {
                logger::warning("Multiple entries matched. Please provide a more specific path.");
                for (std::size_t i = 0; i < std::min<std::size_t>(matches.size(), 10); ++i) {
                    std::cout << " - " << matches[i]->path << "\n";
                }
            }
            return ExitCode::UsageError;
        }

        it = baseline.files.find(matches[0]->path);
    }

    const core::FileEntry& entry = it->second;
    if (as_json) {
        std::cout << "{\n"
                  << "  \"path\": \"" << json_escape(entry.path) << "\",\n"
                  << "  \"hash\": \"" << entry.hash << "\",\n"
                  << "  \"size\": " << entry.size << ",\n"
                  << "  \"mtime\": " << entry.mtime << "\n"
                  << "}\n";
        return ExitCode::Ok;
    }

    std::cout << "Path : " << entry.path << "\n"
              << "Hash : " << entry.hash << "\n"
              << "Size : " << entry.size << " bytes\n"
              << "MTime: " << entry.mtime << "\n";
    return ExitCode::Ok;
}

ExitCode handle_export_baseline(const ParsedArgs& parsed) {
    std::string destination;
    if (!require_single_positional(parsed, "<file>", destination)) {
        return ExitCode::UsageError;
    }

    std::error_code ec;
    if (!fs::exists(config::BASELINE_DB, ec)) {
        logger::error("Baseline file not found: " + config::BASELINE_DB);
        return ExitCode::BaselineMissing;
    }

    const bool overwrite = has_switch(parsed, "overwrite");
    if (fs::exists(destination, ec) && !overwrite) {
        logger::error("Destination already exists. Use --overwrite to replace it.");
        return ExitCode::UsageError;
    }

    const fs::path dest_path(destination);
    if (dest_path.has_parent_path()) {
        fs::create_directories(dest_path.parent_path(), ec);
    }

    const fs::copy_options options =
        overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
    fs::copy_file(config::BASELINE_DB, destination, options, ec);
    if (ec) {
        logger::error("Failed to export baseline: " + ec.message());
        return ExitCode::OperationFailed;
    }

    logger::success("Baseline exported to: " + destination);
    return ExitCode::Ok;
}

ExitCode handle_import_baseline(const ParsedArgs& parsed) {
    std::string source;
    if (!require_single_positional(parsed, "<file>", source)) {
        return ExitCode::UsageError;
    }

    std::error_code ec;
    if (!fs::exists(source, ec)) {
        logger::error("Source baseline file not found: " + source);
        return ExitCode::UsageError;
    }

    const bool force = has_switch(parsed, "force");
    const bool baseline_exists = fs::exists(config::BASELINE_DB, ec);
    if (baseline_exists && !force) {
        logger::error("Baseline already exists. Use --force to replace it.");
        return ExitCode::UsageError;
    }

    const std::string backup_path = config::BASELINE_DB + ".bak";
    if (baseline_exists) {
        fs::copy_file(config::BASELINE_DB, backup_path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            logger::error("Failed to create backup baseline: " + ec.message());
            return ExitCode::OperationFailed;
        }
    }

    fs::copy_file(source, config::BASELINE_DB, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        logger::error("Failed to import baseline: " + ec.message());
        return ExitCode::OperationFailed;
    }

    BaselineView loaded;
    if (load_baseline(loaded) != ExitCode::Ok) {
        if (baseline_exists) {
            fs::copy_file(backup_path, config::BASELINE_DB, fs::copy_options::overwrite_existing, ec);
        }
        logger::error("Imported baseline is invalid.");
        return ExitCode::OperationFailed;
    }

    if (!scanner::save_baseline(loaded.files, loaded.root)) {
        if (baseline_exists) {
            fs::copy_file(backup_path, config::BASELINE_DB, fs::copy_options::overwrite_existing, ec);
        }
        const std::string detail = scanner::baseline_last_error();
        logger::error(detail.empty() ? "Failed to re-seal imported baseline." : detail);
        return ExitCode::OperationFailed;
    }

    fs::remove(backup_path, ec);
    logger::success("Baseline imported successfully.");
    if (!loaded.root.empty()) {
        logger::info("Imported baseline target: " + loaded.root);
    }
    return ExitCode::Ok;
}

} // namespace commands
