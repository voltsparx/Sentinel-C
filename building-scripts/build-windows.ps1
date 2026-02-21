[CmdletBinding()]
param(
    [string]$BuildDir = "build-windows",
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Generator = "",
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [int]$Code = 1
    )
    Write-Error $Message
    exit $Code
}

function Test-CommandAvailable {
    param([Parameter(Mandatory = $true)][string]$CommandName)
    return ($null -ne (Get-Command $CommandName -ErrorAction SilentlyContinue))
}

try {
    $scriptDir = $PSScriptRoot
    if ([string]::IsNullOrWhiteSpace($scriptDir) -and -not [string]::IsNullOrWhiteSpace($PSCommandPath)) {
        $scriptDir = Split-Path -Parent $PSCommandPath
    }
    if ([string]::IsNullOrWhiteSpace($scriptDir)) {
        Fail "Unable to resolve script directory."
    }

    $projectRoot = (Resolve-Path (Join-Path $scriptDir "..")).Path
    $cmakeLists = Join-Path $projectRoot "CMakeLists.txt"
    if (-not (Test-Path $cmakeLists)) {
        Fail "CMakeLists.txt was not found at project root: $projectRoot"
    }

    if (-not (Test-CommandAvailable "cmake")) {
        Fail "Required command 'cmake' was not found in PATH."
    }

    Write-Host "[INFO] Project root: $projectRoot"
    Write-Host "[INFO] Build directory: $BuildDir"
    Write-Host "[INFO] Configuration: $Configuration"
    $buildPath = Join-Path $projectRoot $BuildDir
    if ($Clean -and (Test-Path $buildPath)) {
        Write-Host "[INFO] Cleaning existing build directory: $buildPath"
        Remove-Item -Path $buildPath -Recurse -Force
    }

    $generatorCandidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        $generatorCandidates.Add($Generator)
    } else {
        $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vsWhere) {
            $vsInstall = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
            if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($vsInstall)) {
                $generatorCandidates.Add("Visual Studio 17 2022")
            }
        }
        if (Get-Command ninja -ErrorAction SilentlyContinue) {
            $generatorCandidates.Add("Ninja")
        }
        if (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
            $generatorCandidates.Add("MinGW Makefiles")
        }
        $generatorCandidates.Add("NMake Makefiles")
    }

    $configured = $false
    foreach ($candidate in $generatorCandidates) {
        if (Test-Path $buildPath) {
            Remove-Item -Path $buildPath -Recurse -Force
        }
        New-Item -ItemType Directory -Path $buildPath -Force | Out-Null

        Write-Host "[INFO] Trying generator: $candidate"
        $configureArgs = @("-S", $projectRoot, "-B", $buildPath, "-G", $candidate)
        if ($candidate -in @("Ninja", "NMake Makefiles", "MinGW Makefiles", "Unix Makefiles")) {
            $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
        }

        & cmake @configureArgs
        if ($LASTEXITCODE -eq 0) {
            $Generator = $candidate
            $configured = $true
            break
        }

        if (-not [string]::IsNullOrWhiteSpace($Generator)) {
            break
        }
        Write-Warning "Generator '$candidate' configure failed. Trying next option."
    }

    if (-not $configured) {
        Fail "CMake configure step failed for all attempted generators." 1
    }

    Write-Host "[SUCCESS] Configure completed with generator: $Generator"

    $buildArgs = @("--build", $buildPath, "--target", "sentinel-c")
    if ($Generator -notin @("Ninja", "NMake Makefiles", "MinGW Makefiles", "Unix Makefiles")) {
        $buildArgs += @("--config", $Configuration)
    }
    if ([Environment]::ProcessorCount -gt 1) {
        $buildArgs += @("--parallel", [Environment]::ProcessorCount)
    }

    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        Fail "CMake build step failed." $LASTEXITCODE
    }

    $candidates = @(
        (Join-Path $buildPath "bin\sentinel-c.exe"),
        (Join-Path $buildPath ("bin\" + $Configuration + "\sentinel-c.exe")),
        (Join-Path $buildPath ($Configuration + "\bin\sentinel-c.exe")),
        (Join-Path $buildPath "sentinel-c.exe")
    )

    $binaryPath = $null
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $binaryPath = (Resolve-Path $candidate).Path
            break
        }
    }

    if (-not $binaryPath) {
        Fail "Build finished but sentinel-c.exe was not found in expected locations." 2
    }

    $releaseDir = Join-Path $projectRoot "bin-releases\windows"
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
    $releaseBinary = Join-Path $releaseDir ("sentinel-c-" + $Configuration + ".exe")
    Copy-Item -Path $binaryPath -Destination $releaseBinary -Force

    Write-Host "[SUCCESS] Build completed."
    Write-Host "[SUCCESS] Binary: $binaryPath"
    Write-Host "[SUCCESS] Release copy: $releaseBinary"
    exit 0
} catch {
    Write-Error ("Unhandled build failure: " + $_.Exception.Message)
    exit 1
}
