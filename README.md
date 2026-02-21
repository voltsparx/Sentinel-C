# Sentinel-C v4.0

Author: voltsparx  
Contact: voltsparx@gmail.com

Sentinel-C is a local-first host integrity monitoring framework for defensive operations.
It builds a trusted baseline, detects drift (new/modified/deleted files), and produces
consistent CLI, HTML, and JSON evidence for both humans and automation.

## Core Capabilities

- Baseline creation and strict baseline-target validation
- Recursive integrity scanning with SHA-256 hashing
- Multi-format reporting (CLI ASCII table, HTML, JSON)
- Baseline tamper guard with SHA-256 seal verification
- CI-friendly status and verification workflows with stable exit codes
- Maintenance operations (doctor, purge, tail log, baseline import/export)
- Guidance sentences in terminal and reports with risk, summary, and clear next actions

## Safety and Trust Model

- Local-first: no automatic data upload
- Explicit state changes: baseline update requires an explicit command
- Transparent outcomes: structured summaries, report files, deterministic exit codes
- Ethical use only: run only on systems you own or are authorized to monitor

## Quick Start

```bash
# Build (Linux/macOS/WSL)
mkdir build && cd build
cmake ..
cmake --build .

# Initialize and scan
./bin/sentinel-c --init /path/to/target
./bin/sentinel-c --scan /path/to/target
```

For full platform setup instructions, see `SETUP.md`.

## Automated Binary Build Scripts

Sentinel-C includes platform-specific build automation under `building-scripts/`:

- Windows PowerShell: `building-scripts/build-windows.ps1`
- Linux shell: `building-scripts/build-linux.sh`
- macOS shell: `building-scripts/build-macos.sh`
- Termux (Android): `termux-support/build-termux.sh`

Examples:

```powershell
# Windows (PowerShell)
powershell -ExecutionPolicy Bypass -File .\building-scripts\build-windows.ps1 -Configuration Release -Clean
```

```bash
# Linux
bash building-scripts/build-linux.sh --build-type Release --clean

# macOS
bash building-scripts/build-macos.sh --build-type Release --clean

# Termux (Android)
bash termux-support/build-termux.sh
```

Each script:

- validates required tools (`cmake`, compiler)
- handles configure/build failures with explicit error messages
- verifies the output binary exists after build
- copies the built binary into `bin-releases/<platform>/releases/bin/`

## Command Reference

### Major Commands

- `--init <path>`: initialize baseline (`--force`, `--json`)
- `--scan <path>`: compare with baseline and generate reports (`--json`)
- `--update <path>`: scan and refresh baseline (`--json`)
- `--status <path>`: CI-focused integrity status (`--json`)
- `--verify <path>`: strict verification (`--reports`, `--json`)
- `--watch <path>`: interval monitoring (`--interval N`, `--cycles N`, `--reports`, `--fail-fast`, `--json`)
- `--doctor`: environment and storage health checks (`--fix`, `--json`)
- `--guard`: security-focused hardening and baseline integrity checks (`--fix`, `--json`)
- `--list-baseline`: list tracked baseline entries (`--limit N`, `--json`)
- `--show-baseline <path>`: inspect one baseline entry (`--json`)
- `--purge-reports`: report retention cleanup (`--days N`, `--all`, `--dry-run`)

### Utility Commands

- `--export-baseline <file>` (`--overwrite`)
- `--import-baseline <file>` (`--force`)
- `--tail-log` (`--lines N`)
- `--report-index` (`--type all|cli|html|json|csv`, `--limit N`, `--json`)
- `--prompt-mode` (`--target`, `--interval`, `--cycles`, `--reports`, `--report-formats`, `--strict`, `--hash-only`, `--quiet`, `--no-advice`)
- `--version` (`--json`)
- `--about`
- `--explain`
- `--help`
- `--output-root <path>` (available on operational commands to set log/report/baseline destination)

Prompt-only keywords:
- `banner` (clears screen, then prints banner)
- `clear` (clears the console)
- `exit` or `Ctrl+C` (leaves prompt mode)

## Exit Codes

- `0`: success / clean state
- `1`: usage or argument error
- `2`: integrity changes detected
- `3`: baseline missing
- `4`: baseline target mismatch
- `5`: operation failed

## Output Layout

Sentinel-C writes into `sentinel-c-logs/` under the binary directory by default.
You can override destination per command using `--output-root <path>`, or globally
using `SENTINEL_ROOT`.

- `sentinel-c-logs/data/.sentinel-baseline`
- `sentinel-c-logs/data/.sentinel-baseline.seal`
- `sentinel-c-logs/logs/sentinel-c_activity_log_<YYYYMMDD_HHMMSS_mmm>.log`
- `sentinel-c-logs/reports/cli/sentinel-c_integrity_cli_report_<YYYYMMDD_HHMMSS_mmm>.txt`
- `sentinel-c-logs/reports/html/sentinel-c_integrity_html_report_<YYYYMMDD_HHMMSS_mmm>.html`
- `sentinel-c-logs/reports/json/sentinel-c_integrity_json_report_<YYYYMMDD_HHMMSS_mmm>.json`
- `sentinel-c-logs/reports/csv/sentinel-c_integrity_csv_report_<YYYYMMDD_HHMMSS_mmm>.csv`

Terminal summaries print absolute output paths for easy navigation.

## Operational Notes (Important Corners)

- Baseline is bound to its initialized target root path.
- Scanning a different path with an existing baseline returns target mismatch (`4`).
- Replacing an existing baseline requires `--init <path> --force`.
- `--status`, `--verify`, and `--watch` return `2` when drift is detected.
- Use `--json` for machine pipelines; use CLI/HTML for analyst review.

## Project Layout

```
Sentinel-C/
  src/
    commands/   # command parsing and handlers
    core/       # config, logging, summary, filesystem helpers
    scanner/    # snapshot, baseline, ignore, hash
    reports/    # CLI/HTML/JSON writers + report advisor
  building-scripts/
    build-windows.ps1
    build-linux.sh
    build-macos.sh
  termux-support/
    build-termux.sh
    Setup.txt
    Usage.txt
  docs/
    Usage.txt
  ARCHITECTURE.md
  SETUP.md
  CMakeLists.txt
```

## Documentation

- `docs/Usage.txt`: plain-text usage guide
- `termux-support/Setup.txt`: plain-text Termux setup guide
- `termux-support/Usage.txt`: plain-text Termux usage guide
- `SETUP.md`: build/install instructions
- `ARCHITECTURE.md`: module boundaries, data flow, concurrency model
- `building-scripts/`: automated platform build scripts with error handling

## License

MIT License
