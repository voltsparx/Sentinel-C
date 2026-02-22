# Sentinel-C v4.5 - Build & Setup Guide

## ⚠️ Requirements

Before building Sentinel-C, ensure you have:

1. **CMake** (version 3.10 or higher)
   - Download: https://cmake.org/download/

2. **C++ Compiler** (C++17 support required)
   - **Windows**: Visual Studio 2015+ OR MinGW
   - **Linux**: GCC or Clang
   - **macOS**: Xcode Command Line Tools

3. **Git** (optional, for cloning the repository)

---

## 🚀 Automated Build Scripts (Recommended)

Sentinel-C provides platform scripts in `building-scripts/` with strict error handling.
Each script validates tools, runs configure + build, verifies the binary, and copies it
to `bin-releases/<platform>/releases/bin/`.

### Windows (PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File .\building-scripts\build-windows.ps1 -Configuration Release -Clean
```

Optional parameters:
- `-BuildDir build-windows`
- `-Configuration Release|Debug|RelWithDebInfo|MinSizeRel`
- `-Generator "Visual Studio 17 2022"` (or another installed generator)
- `-Clean`

### Linux

```bash
bash building-scripts/build-linux.sh --build-type Release --clean
```

Optional parameters:
- `--build-dir build-linux`
- `--build-type Release|Debug|RelWithDebInfo|MinSizeRel`
- `--generator "<cmake-generator>"`
- `--jobs <n>`
- `--clean`

### macOS

```bash
bash building-scripts/build-macos.sh --build-type Release --clean
```

Optional parameters:
- `--build-dir build-macos`
- `--build-type Release|Debug|RelWithDebInfo|MinSizeRel`
- `--generator "<cmake-generator>"`
- `--jobs <n>`
- `--clean`

### Termux (Android)

```bash
bash termux-support/build-termux.sh
```

Optional parameters:
- `--build-dir build-termux`
- `--build-type Release|Debug|RelWithDebInfo|MinSizeRel`
- `--jobs <n>`
- `--clean`
- `--yes` (auto-approve missing package installs)
- `--no-install` (fail if required tools are missing)

---

## 🪟 Windows Build Instructions

### Option 1: Visual Studio 2022 (Recommended)

1. Install **Visual Studio 2022** with C++ development tools
2. Clone/extract the repository
3. Open PowerShell or Command Prompt
4. Navigate to the project directory:
   ```powershell
   cd Sentinel-C-main
   ```

5. Create and enter build directory:
   ```powershell
   mkdir build
   cd build
   ```

6. Generate Visual Studio project files:
   ```powershell
   cmake -G "Visual Studio 17 2022" ..
   ```

7. Build the project:
   ```powershell
   cmake --build . --config Release
   ```

8. The executable will be in: `build/bin/sentinel-c.exe`

---

### Option 2: MinGW Compiler

1. Install **MinGW-w64** from https://www.mingw-w64.org/
2. Ensure `g++` and `mingw32-make` are in your PATH
3. Clone/extract the repository
4. Navigate to project directory and create build folder:
   ```powershell
   mkdir build
   cd build
   ```

5. Configure with MinGW:
   ```powershell
   cmake -G "MinGW Makefiles" ..
   ```

6. Build:
   ```powershell
   cmake --build .
   ```

7. The executable will be in: `build/bin/sentinel-c.exe`

---

## 🐧 Linux Build Instructions

1. Install required build tools (Ubuntu/Debian):
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake
   ```

2. Install for Fedora/RedHat:
   ```bash
   sudo dnf install gcc-c++ cmake
   ```

3. Clone the repository:
   ```bash
   git clone https://github.com/voltsparx/Sentinel-C.git
   cd Sentinel-C
   ```

4. Create and enter build directory:
   ```bash
   mkdir build
   cd build
   ```

5. Generate build files:
   ```bash
   cmake ..
   ```

6. Build the project:
   ```bash
   cmake --build .
   ```

7. The executable will be in: `build/bin/sentinel-c`

8. (Optional) Add to PATH:
   ```bash
   sudo cp build/bin/sentinel-c /usr/local/bin/
   ```

---

## 🍎 macOS Build Instructions

1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```

2. Install CMake (using Homebrew):
   ```bash
   brew install cmake
   ```

3. Clone the repository:
   ```bash
   git clone https://github.com/voltsparx/Sentinel-C.git
   cd Sentinel-C
   ```

4. Create and enter build directory:
   ```bash
   mkdir build
   cd build
   ```

5. Generate build files:
   ```bash
   cmake ..
   ```

6. Build the project:
   ```bash
   cmake --build .
   ```

7. The executable will be in: `build/bin/sentinel-c`

---

## ✅ Verify Installation

After building, verify the executable works:

**Windows:**
```powershell
./build/bin/sentinel-c.exe --help
```

**Linux/macOS:**
```bash
./build/bin/sentinel-c --help
```

Expected output:
```
Sentinel-C v4.5

--init <path>           Initialize baseline
--scan <path>           Scan and report changes (CLI/HTML/JSON/CSV)
--update <path>         Scan and refresh baseline
--status <path>         CI-friendly integrity status
--verify <path>         Verification workflow (optional reports)
--watch <path>          Continuous interval-based monitoring
--doctor                Environment and storage diagnostics
--guard                 Security-focused hardening checks
--list-baseline         List tracked baseline entries
--show-baseline <path>  Show a specific baseline entry
--export-baseline <f>   Export baseline to file
--import-baseline <f>   Import baseline from file
--purge-reports         Remove old report artifacts
--tail-log              Tail Sentinel-C log output
--report-index          List report artifacts (supports JSON output)
--prompt-mode           Guided interactive console mode
--set-destination       Save destination root for future runs
--show-destination      Show active and saved destination settings
--version               Show version metadata
--about                 Show trust-focused tool overview
--explain               Explain major flags with examples
--help                  Show help
```

Output destination:
- Default runtime storage path is alongside the built binary (`<binary-dir>/sentinel-c-logs`).
- You can override per command using `--output-root <path>`.
- You can persist destination using `--set-destination <path>`.
- Logs/reports are written with timestamped meaningful filenames.

---

## 🔧 Troubleshooting

### Error: "no such file or directory" for nmake/compiler

**Cause**: No C++ compiler detected on your system

**Solution**: 
- **Windows**: Install Visual Studio 2022 or MinGW
- **Linux**: Run `sudo apt-get install build-essential`
- **macOS**: Run `xcode-select --install`

### Error: "CMAKE_CXX_COMPILER not set"

**Solution**: Specify the compiler path explicitly:
```bash
cmake -D CMAKE_CXX_COMPILER=<path-to-compiler> ..
```

### CMake "could not find any instance of Visual Studio"

**Solution**: Use MinGW generator instead:
```powershell
cmake -G "MinGW Makefiles" ..
```

---

## 📝 Build Configuration Options

You can customize the build by adding flags to `cmake`:

```bash
# Build in Debug mode (with debug symbols)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build in Release mode (optimized)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Specify custom C++ compiler
cmake -D CMAKE_CXX_COMPILER=clang++ ..
```

---

## 🧹 Cleaning the Build

To clean and rebuild:

```bash
cd build
rm -r * (Linux/macOS)
rmdir /s /q * (Windows)
cmake ..
cmake --build .
```

---

## 📚 Quick Start After Building

```bash
# Initialize baseline for a directory
./sentinel-c --init /path/to/directory

# Scan for changes
./sentinel-c --scan /path/to/directory

# Run health diagnostics
./sentinel-c --doctor

# Use CI-friendly status check
./sentinel-c --status /path/to/directory

# View reports in sentinel-c-logs/reports/
```

---

## 💡 Additional Notes

- The project uses **C++17 standard**
- Cross-platform compatible
- Requires **at least 50MB** of free disk space for build artifacts
- Reports are generated in `sentinel-c-logs/` directory
