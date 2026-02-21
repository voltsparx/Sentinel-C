#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

on_error() {
  local exit_code=$?
  echo "[ERROR] Termux build failed at line ${1}: ${2}" >&2
  exit "${exit_code}"
}
trap 'on_error ${LINENO} "${BASH_COMMAND}"' ERR

BUILD_DIR="build-termux"
BUILD_TYPE="Release"
JOBS=""
CLEAN=0
AUTO_YES=0
NO_INSTALL=0

usage() {
  cat <<'EOF'
Usage: bash termux-support/build-termux.sh [options]

Options:
  --build-dir <name>     Build directory (default: build-termux)
  --build-type <type>    CMake build type (default: Release)
  --jobs <n>             Parallel build jobs
  --clean                Remove build directory before configure
  --yes                  Auto-approve missing tool installation
  --no-install           Do not install missing tools
  --help                 Show this help
EOF
}

log_info() {
  echo "[INFO] $*"
}

log_warn() {
  echo "[WARN] $*"
}

log_error() {
  echo "[ERROR] $*" >&2
}

is_termux() {
  [[ -n "${PREFIX:-}" && "${PREFIX}" == *"/com.termux/"* ]]
}

confirm() {
  local prompt="$1"
  if [[ "${AUTO_YES}" -eq 1 ]]; then
    return 0
  fi
  read -r -p "${prompt} [y/N]: " reply
  case "${reply}" in
    y|Y|yes|YES) return 0 ;;
    *) return 1 ;;
  esac
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
    --jobs)
      JOBS="${2:-}"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --yes)
      AUTO_YES=1
      shift
      ;;
    --no-install)
      NO_INSTALL=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      log_error "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_PATH="${PROJECT_ROOT}/${BUILD_DIR}"

if ! is_termux; then
  log_error "This script is for Termux only."
  log_error "Run it inside Termux on Android."
  exit 1
fi

if [[ ! -f "${PROJECT_ROOT}/CMakeLists.txt" ]]; then
  log_error "CMakeLists.txt not found at project root: ${PROJECT_ROOT}"
  exit 1
fi

log_info "Project root: ${PROJECT_ROOT}"
log_info "Build directory: ${BUILD_DIR}"
log_info "Build type: ${BUILD_TYPE}"

declare -a REQUIRED_TOOLS=("cmake" "clang++" "make")
declare -A TOOL_PACKAGE
TOOL_PACKAGE["cmake"]="cmake"
TOOL_PACKAGE["clang++"]="clang"
TOOL_PACKAGE["make"]="make"

declare -a MISSING_TOOLS=()
for tool in "${REQUIRED_TOOLS[@]}"; do
  if command -v "${tool}" >/dev/null 2>&1; then
    log_info "Found tool: ${tool}"
  else
    log_warn "Missing tool: ${tool}"
    MISSING_TOOLS+=("${tool}")
  fi
done

if [[ "${#MISSING_TOOLS[@]}" -gt 0 ]]; then
  declare -a PACKAGES=()
  declare -A SEEN
  for tool in "${MISSING_TOOLS[@]}"; do
    package="${TOOL_PACKAGE[${tool}]}"
    if [[ -z "${SEEN[${package}]:-}" ]]; then
      PACKAGES+=("${package}")
      SEEN["${package}"]=1
    fi
  done

  if [[ "${NO_INSTALL}" -eq 1 ]]; then
    log_error "Required tools are missing and --no-install was set."
    log_error "Install packages manually: pkg install -y ${PACKAGES[*]}"
    exit 1
  fi

  if ! command -v pkg >/dev/null 2>&1; then
    log_error "Termux package manager 'pkg' was not found."
    log_error "Install missing packages manually: ${PACKAGES[*]}"
    exit 1
  fi

  log_warn "Missing tools require packages: ${PACKAGES[*]}"
  if ! confirm "Install missing packages now using pkg?"; then
    log_error "Build stopped. Install required packages and run again."
    exit 1
  fi

  log_info "Updating package index..."
  pkg update -y
  log_info "Installing: ${PACKAGES[*]}"
  pkg install -y "${PACKAGES[@]}"

  for tool in "${MISSING_TOOLS[@]}"; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
      log_error "Tool still missing after install: ${tool}"
      exit 1
    fi
  done
fi

if [[ "${CLEAN}" -eq 1 ]]; then
  log_info "Cleaning build directory: ${BUILD_PATH}"
  rm -rf "${BUILD_PATH}"
fi

if [[ -e "${BUILD_PATH}" && ! -d "${BUILD_PATH}" ]]; then
  log_error "Build path exists but is not a directory: ${BUILD_PATH}"
  exit 1
fi

mkdir -p "${BUILD_PATH}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_PATH}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

if [[ -n "${JOBS}" ]]; then
  cmake --build "${BUILD_PATH}" --target sentinel-c --parallel "${JOBS}"
else
  if command -v nproc >/dev/null 2>&1; then
    cmake --build "${BUILD_PATH}" --target sentinel-c --parallel "$(nproc)"
  else
    cmake --build "${BUILD_PATH}" --target sentinel-c --parallel
  fi
fi

BINARY_PATH="${BUILD_PATH}/bin/sentinel-c"
if [[ ! -x "${BINARY_PATH}" ]]; then
  log_error "Build completed but binary not found: ${BINARY_PATH}"
  exit 2
fi

RELEASE_DIR="${PROJECT_ROOT}/bin-releases/termux/releases"
RELEASE_BIN_DIR="${RELEASE_DIR}/bin"
mkdir -p "${RELEASE_BIN_DIR}"

cp "${BINARY_PATH}" "${RELEASE_BIN_DIR}/sentinel-c"
chmod +x "${RELEASE_BIN_DIR}/sentinel-c"

if [[ -f "${PROJECT_ROOT}/LICENSE" ]]; then
  cp "${PROJECT_ROOT}/LICENSE" "${RELEASE_DIR}/LICENSE"
fi
if [[ -f "${SCRIPT_DIR}/Usage.txt" ]]; then
  cp "${SCRIPT_DIR}/Usage.txt" "${RELEASE_DIR}/Usage.txt"
fi
if [[ -f "${SCRIPT_DIR}/Setup.txt" ]]; then
  cp "${SCRIPT_DIR}/Setup.txt" "${RELEASE_DIR}/Setup.txt"
fi

if command -v sha256sum >/dev/null 2>&1; then
  (
    cd "${RELEASE_DIR}"
    find . -type f ! -name SHA256SUMS.txt -print0 \
      | sort -z \
      | xargs -0 sha256sum > SHA256SUMS.txt
  )
fi

log_info "Build completed successfully."
log_info "Binary: ${BINARY_PATH}"
log_info "Release binary: ${RELEASE_BIN_DIR}/sentinel-c"
log_info "Release root: ${RELEASE_DIR}"
