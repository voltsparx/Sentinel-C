#!/usr/bin/env bash
set -Eeuo pipefail

on_error() {
  local exit_code=$?
  echo "[ERROR] Linux build failed at line ${1}: ${2}" >&2
  exit "${exit_code}"
}
trap 'on_error ${LINENO} "${BASH_COMMAND}"' ERR

BUILD_DIR="build-linux"
BUILD_TYPE="Release"
GENERATOR=""
JOBS=""
CLEAN=0

usage() {
  cat <<'EOF'
Usage: bash building-scripts/build-linux.sh [options]

Options:
  --build-dir <name>     Build directory (default: build-linux)
  --build-type <type>    CMake build type (default: Release)
  --generator <name>     CMake generator override
  --jobs <n>             Parallel build jobs
  --clean                Remove build directory before configure
  --help                 Show this help
EOF
}

require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "[ERROR] Required command '${cmd}' not found in PATH." >&2
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="${2:-}"
      shift 2
      ;;
    --generator)
      GENERATOR="${2:-}"
      shift 2
      ;;
    --jobs)
      JOBS="${2:-}"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "[ERROR] Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_PATH="${PROJECT_ROOT}/${BUILD_DIR}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "[ERROR] This script is for Linux only. Use build-macos.sh on macOS." >&2
  exit 1
fi

if [[ ! -f "${PROJECT_ROOT}/CMakeLists.txt" ]]; then
  echo "[ERROR] CMakeLists.txt not found at project root: ${PROJECT_ROOT}" >&2
  exit 1
fi

require_command cmake
if ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
  echo "[ERROR] No C++ compiler detected (c++, g++, or clang++)." >&2
  exit 1
fi

echo "[INFO] Project root: ${PROJECT_ROOT}"
echo "[INFO] Build directory: ${BUILD_DIR}"
echo "[INFO] Build type: ${BUILD_TYPE}"
if [[ -n "${GENERATOR}" ]]; then
  echo "[INFO] Generator override: ${GENERATOR}"
fi

if [[ "${CLEAN}" -eq 1 ]]; then
  echo "[INFO] Cleaning build directory: ${BUILD_PATH}"
  rm -rf "${BUILD_PATH}"
fi

if [[ -e "${BUILD_PATH}" && ! -d "${BUILD_PATH}" ]]; then
  echo "[ERROR] Build path exists but is not a directory: ${BUILD_PATH}" >&2
  echo "[ERROR] Remove/rename that file or choose another build dir with --build-dir <name>." >&2
  exit 1
fi

mkdir -p "${BUILD_PATH}"

CONFIGURE_ARGS=(-S "${PROJECT_ROOT}" -B "${BUILD_PATH}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
if [[ -n "${GENERATOR}" ]]; then
  CONFIGURE_ARGS+=(-G "${GENERATOR}")
fi

cmake "${CONFIGURE_ARGS[@]}"

BUILD_ARGS=(--build "${BUILD_PATH}" --target sentinel-c)
if [[ -n "${JOBS}" ]]; then
  BUILD_ARGS+=(--parallel "${JOBS}")
else
  BUILD_ARGS+=(--parallel)
fi

cmake "${BUILD_ARGS[@]}"

BINARY_PATH="${BUILD_PATH}/bin/sentinel-c"
if [[ ! -x "${BINARY_PATH}" ]]; then
  echo "[ERROR] Build succeeded but binary not found: ${BINARY_PATH}" >&2
  exit 2
fi

RELEASE_DIR="${PROJECT_ROOT}/bin-releases/linux/releases"
RELEASE_BIN_DIR="${RELEASE_DIR}/bin"
RELEASE_DOCS_DIR="${RELEASE_DIR}/docs"

if [[ -e "${RELEASE_DIR}" && ! -d "${RELEASE_DIR}" ]]; then
  echo "[ERROR] Release path exists but is not a directory: ${RELEASE_DIR}" >&2
  exit 1
fi

mkdir -p "${RELEASE_BIN_DIR}"
mkdir -p "${RELEASE_DOCS_DIR}"

RELEASE_BINARY="${RELEASE_BIN_DIR}/sentinel-c"
cp "${BINARY_PATH}" "${RELEASE_BINARY}"
chmod +x "${RELEASE_BINARY}"

if [[ -f "${PROJECT_ROOT}/LICENSE" ]]; then
  cp "${PROJECT_ROOT}/LICENSE" "${RELEASE_DIR}/LICENSE"
fi

if [[ -f "${PROJECT_ROOT}/docs/Usage.txt" ]]; then
  cp "${PROJECT_ROOT}/docs/Usage.txt" "${RELEASE_DIR}/Usage.txt"
fi

if [[ -d "${PROJECT_ROOT}/docs" ]]; then
  rm -rf "${RELEASE_DOCS_DIR}"
  mkdir -p "${RELEASE_DOCS_DIR}"
  cp -r "${PROJECT_ROOT}/docs/." "${RELEASE_DOCS_DIR}/"
fi

cat > "${RELEASE_DIR}/README.md" <<'EOF'
# Sentinel-C v4.5 (Linux x86_64)

Author: voltsparx
Contact: voltsparx@gmail.com

Contents:
- bin/sentinel-c
- docs/
- Usage.txt
- LICENSE
- SHA256SUMS.txt

Quick Run:
chmod +x bin/sentinel-c
./bin/sentinel-c --help
./bin/sentinel-c --set-destination "$HOME/sentinel-c-data"
./bin/sentinel-c --prompt-mode
EOF

if command -v sha256sum >/dev/null 2>&1; then
  (
    cd "${RELEASE_DIR}"
    find . -type f ! -name SHA256SUMS.txt -print0 \
      | sort -z \
      | xargs -0 sha256sum > SHA256SUMS.txt
  )
fi

echo "[SUCCESS] Build completed."
echo "[SUCCESS] Binary: ${BINARY_PATH}"
echo "[SUCCESS] Release copy: ${RELEASE_BINARY}"
echo "[SUCCESS] Release root: ${RELEASE_DIR}"
