<# .SYNOPSIS
   Bootstrap vcpkg + download pre-built libmpv for a Windows MSVC build.

   Run from the project root in PowerShell:
     .\scripts\setup-windows-vcpkg.ps1

   Prerequisites:
     - Visual Studio 2022 Build Tools (or full VS) with C++ workload
     - CMake 3.21+ and Ninja on PATH
     - Qt 6.5+ installed (e.g. via aqtinstall or Qt Online Installer)
     - 7-Zip or tar (for extracting mpv-dev archive)

   What this script does:
     1. Clones vcpkg into ./vcpkg (if not already present) and bootstraps it.
     2. Downloads the latest mpv-dev SDK from SourceForge into ./external/mpv-dev.
     3. Creates an MSVC import library (mpv.lib) from the shipped .def file.
#>

param(
    [string]$MpvDevUrl = "https://sourceforge.net/projects/mpv-player-windows/files/libmpv/mpv-dev-x86_64-v3-20260419-git-06f4ce7.7z/download",
    [switch]$SkipVcpkg,
    [switch]$SkipMpv
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $ProjectRoot

# ---------- 1. vcpkg ----------------------------------------------------------
if (-not $SkipVcpkg) {
    if (-not (Test-Path "vcpkg\.vcpkg-root")) {
        Write-Host "==> Cloning vcpkg..." -ForegroundColor Green
        git clone https://github.com/microsoft/vcpkg.git vcpkg
    }
    Write-Host "==> Bootstrapping vcpkg..." -ForegroundColor Green
    & .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
    Write-Host "==> vcpkg ready." -ForegroundColor Green
}

# ---------- 2. Pre-built libmpv -----------------------------------------------
if (-not $SkipMpv) {
    $mpvDir = "external\mpv-dev"
    if (Test-Path "$mpvDir\include\mpv\client.h") {
        Write-Host "==> mpv-dev already present in $mpvDir, skipping download." -ForegroundColor Yellow
    } else {
        New-Item -ItemType Directory -Path "external" -Force | Out-Null
        $archive = "external\mpv-dev.7z"

        Write-Host "==> Downloading mpv-dev SDK..." -ForegroundColor Green
        # SourceForge redirect: curl follows it with -L
        & curl.exe -L -o $archive $MpvDevUrl
        if ($LASTEXITCODE -ne 0) { throw "Download failed" }

        Write-Host "==> Extracting..." -ForegroundColor Green
        if (Get-Command 7z -ErrorAction SilentlyContinue) {
            & 7z x $archive -o"$mpvDir" -y
        } else {
            # tar in Windows 10+ can handle 7z via libarchive
            & tar xf $archive -C external
            Rename-Item "external\mpv-dev-*" "mpv-dev" -ErrorAction SilentlyContinue
        }
        Remove-Item $archive -ErrorAction SilentlyContinue

        # ---------- 3. Create MSVC import lib from .def -----------------------
        $defFile = Get-ChildItem "$mpvDir" -Filter "*.def" -Recurse | Select-Object -First 1
        if ($defFile) {
            Write-Host "==> Creating mpv.lib from $($defFile.Name)..." -ForegroundColor Green
            $libDir = "$mpvDir\lib"
            New-Item -ItemType Directory -Path $libDir -Force | Out-Null
            & lib /def:"$($defFile.FullName)" /out:"$libDir\mpv.lib" /machine:x64
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "lib.exe failed — make sure you run from a VS Developer Command Prompt."
            }
        } else {
            Write-Warning "No .def file found in mpv-dev archive. You may need to create mpv.lib manually."
        }

        Write-Host "==> mpv-dev ready at $mpvDir" -ForegroundColor Green
    }
}

# ---------- Summary -----------------------------------------------------------
Write-Host ""
Write-Host "Setup complete. Build with:" -ForegroundColor Green
Write-Host "  cmake --preset windows-vcpkg"
Write-Host "  cmake --build build-vcpkg -j"
Write-Host ""
Write-Host "Or manually:" -ForegroundColor Green
Write-Host "  cmake -G Ninja -B build-vcpkg ^"
Write-Host "    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^"
Write-Host "    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md ^"
Write-Host "    -DMPV_DIR=external/mpv-dev ^"
Write-Host "    -DCMAKE_BUILD_TYPE=Release"
