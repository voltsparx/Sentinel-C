# Sentinel-C Architecture

## Design Goals

- Reliable host-based integrity detection
- Clear operational outputs for both humans and automation
- Predictable command behavior and exit codes
- Practical performance with bounded concurrency
- Modular source layout for maintainability

## Build Boundary

`CMakeLists.txt` builds the modular tree under `src/`:

- `src/core`: config, logging, summary, filesystem helpers
- `src/commands`: parsing, dispatching, command workflows
- `src/scanner`: snapshot creation, baseline IO, ignore rules, hashing, comparison
- `src/reports`: CLI/HTML/JSON report generation and report-level advisor
- `src/reports`: CLI/HTML/JSON/CSV report generation and report-level advisor
- `src/cli.cpp`: top-level command orchestration

Legacy flat source files under `src/` are intentionally excluded from the main target.

## Build Automation Scripts

Repository-level build helpers are stored in `building-scripts/`:

- `build-windows.ps1`: PowerShell automation for Windows
- `build-linux.sh`: shell automation for Linux
- `build-macos.sh`: shell automation for macOS

These scripts standardize configure/build/verification steps and copy binaries
to `bin-releases/<platform>/`.

## Runtime Flow

1. `main.cpp` calls `cli::parse`.
2. CLI bootstraps directories and logger.
3. Argument parser tokenizes command, switches, and options.
4. Dispatcher enforces allowed flags per command.
5. Command handler runs scan/baseline/maintenance/prompt workflow.
6. Results are summarized in terminal and optionally serialized to reports.

## Module Responsibilities

### `commands`

- `arg_parser.*`: raw token parsing and value extraction
- `dispatcher.*`: command routing + option policy
- `scan_ops.*`: init/scan/update/status/verify/watch
- `baseline_ops.*`: list/show/export/import baseline workflows
- `maintenance_ops.*`: doctor/purge/tail operations
- `prompt_console.*`: beginner-friendly interactive console routed through existing dispatch
- `common.*`: shared helpers, validation, JSON helpers, usage/about/explain
- `advisor.*`: terminal-side nano advisor messages

### `scanner`

- `scanner.*`: snapshot build and baseline diff logic
- `baseline.cpp`: baseline read/write format handling
- `ignore.cpp`: ignore rule loading and matching
- `hash.cpp`: streamed SHA-256 file hashing

### `reports`

- `cli_report.cpp`: plaintext report with ASCII table + advisor section
- `html_report.cpp`: analyst-friendly HTML report + advisor section
- `json_report.cpp`: machine-oriented report + advisor object
- `csv_report.cpp`: pipeline-friendly CSV report with change rows + advisor rows
- `advice.cpp`: shared report guidance generation (summary, why, what-matters, teaching)

## Data Contracts

### Baseline file format (v2)

Stored at `sentinel-c-logs/data/.sentinel-baseline`:

- Header metadata:
  - `root\t<path>`
  - `generated\t<timestamp>`
- File records:
  - `file\t<path>\t<sha256>\t<size>\t<mtime>`

Backward compatibility for legacy `path|size|hash` entries remains supported.

### Scan result model

`scanner::ScanResult` carries:

- `stats`: scanned, added, modified, deleted, duration
- `current`: snapshot map
- `added`, `modified`, `deleted`: diff maps keyed by absolute path

## Concurrency Model

- Snapshot build:
  - Directory walking stays single-threaded and deterministic.
  - Hashing is parallelized with a bounded worker pool when workload is meaningful.
  - Small snapshots stay single-threaded to avoid thread overhead.

- Report generation:
  - CLI, HTML, JSON, and CSV writers are launched concurrently for the same scan id.
  - Output file names remain aligned across report types.

## Reliability Guardrails

- Baseline target validation blocks accidental cross-target scans.
- Missing baseline maps to explicit exit code (`3`).
- Target mismatch maps to explicit exit code (`4`).
- Usage/option errors map to explicit exit code (`1`).
- Changes detected map to explicit exit code (`2`) for CI workflows.
- Operation failures map to explicit exit code (`5`).
- `--strict` extends change-triggered exit code behavior to scan/update.
- `--hash-only` allows mtime drift suppression for noisy environments.

## Extensibility Guidance

- New command:
  - Add parser validation policy in `dispatcher.cpp`.
  - Implement handler in `commands/*_ops.cpp`.
  - Reuse `common.*` helper paths and JSON conventions.

- New report format:
  - Add writer in `src/reports`.
  - Keep same scan id contract for cross-report traceability.
  - Add to CMake source list.

- New scanner signal:
  - Extend `core::FileEntry`/`ScanResult` carefully.
  - Update report serializers and baseline compatibility strategy together.
