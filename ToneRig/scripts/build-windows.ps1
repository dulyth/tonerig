<#
    Builds ToneRig on Windows (x64) - one self-contained VST3 for REAPER.
    Not signed or notarised: REAPER loads unsigned VST3s without complaint.

    Requirements (all free):
      * CMake 3.22+ and Git on PATH
      * Visual Studio 2022 (or the Build Tools) with the
        "Desktop development with C++" workload
        -- or Ninja plus any C++17 compiler (MinGW/clang-cl/MSVC)

    Usage:
      .\scripts\build-windows.ps1                  # configure, build, run DSP tests
      .\scripts\build-windows.ps1 -Clean           # wipe build\ first
      .\scripts\build-windows.ps1 -Install         # also copy into the common VST3 folder
      .\scripts\build-windows.ps1 -Effect          # build as an effect, not a VSTi
      .\scripts\build-windows.ps1 -Generator Ninja # force a specific generator

    If PowerShell refuses to run the script:
      powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
#>
[CmdletBinding()]
param(
    [switch] $Clean,
    [switch] $Install,
    [switch] $Effect,
    [string] $Generator = "",
    [string] $Config    = "Release"
)

$ErrorActionPreference = "Stop"

# Runs a native tool and aborts the script if it fails. PowerShell does not do
# this by itself: a non-zero exit code from cmake/git is *not* terminating.
function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]   $Exe,
        [string[]] $Arguments = @()
    )

    Write-Host ("> {0} {1}" -f $Exe, ($Arguments -join " ")) -ForegroundColor DarkGray
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("'{0}' failed with exit code {1}." -f $Exe, $LASTEXITCODE)
    }
}

function Resolve-Generator {
    # Asks CMake which generators it actually supports and prefers the newest
    # Visual Studio one, so this keeps working on VS 2026 / CMake 4.x.
    # $HelpText exists so the parser can be unit tested without CMake.
    param([string] $HelpText = "")

    if (-not $HelpText) { $HelpText = (& cmake --help) -join "`n" }

    $names = New-Object System.Collections.Generic.List[string]
    $inList = $false

    foreach ($line in ($HelpText -split "`r?`n")) {
        if ($line -match '^\s*Generators\s*$') { $inList = $true; continue }
        if (-not $inList) { continue }

        # A new single-word top-level header ends the list. The descriptive
        # sentence CMake 4 prints before the list is ignored: it is longer
        # than a bare header.
        if ($line -match '^[A-Za-z][A-Za-z ]{0,30}$') { break }

        # Entries look like "Name = description", optionally starred at
        # column 0 when it is the default generator. Option lines look the
        # same, but every CMake option name starts with '-'.
        if ($line -notmatch '^\s*\*?\s*(\S[^=]*?)\s+=') { continue }

        $name = $Matches[1].Trim()
        if ($name -and (-not $name.StartsWith("-"))) { $names.Add($name) }
    }

    # Highest version number wins, so this keeps working on future VS releases.
    $vs = @($names |
        Where-Object { $_ -like "Visual Studio*" } |
        Sort-Object -Property { if ($_ -match '\d+') { [int] $Matches[0] } else { 0 } } -Descending)
    if ($vs.Count -gt 0) { return $vs[0] }
    if ($names -contains "Ninja") { return "Ninja" }
    if ($names.Count -gt 0) { return $names[0] }
    return ""
}

$Root  = Split-Path -Parent $PSScriptRoot
$Juce  = Join-Path $Root "third_party\JUCE"
$Build = Join-Path $Root "build"

foreach ($tool in @("cmake", "git")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw ("'{0}' was not found on PATH. Install it and reopen the terminal." -f $tool)
    }
}

Write-Host "== ToneRig Windows build ==" -ForegroundColor Cyan

# ---------------------------------------------------------------- JUCE ------
if (-not (Test-Path (Join-Path $Juce "modules"))) {
    Write-Host "Fetching JUCE 8.0.15 into third_party\JUCE ..."
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Juce) | Out-Null
    Invoke-Native "git" @("clone", "--depth", "1", "--branch", "8.0.15",
                          "https://github.com/juce-framework/JUCE.git", $Juce)
} else {
    Write-Host "Using JUCE already present in third_party\JUCE"
}

# ------------------------------------------------------------- clean --------
if ($Clean -and (Test-Path $Build)) {
    Write-Host "Removing $Build"
    Remove-Item -Recurse -Force $Build
}

# ------------------------------------------------------ generator ----------
$gen = $Generator
if (-not $gen) {
    $gen = Resolve-Generator
    if (-not $gen) {
        throw "Could not find a usable CMake generator. Install Visual Studio 2022 (Desktop development with C++) or Ninja."
    }
    Write-Host "Auto-selected CMake generator: $gen"
} else {
    Write-Host "Using requested CMake generator: $gen"
}

# A build tree remembers its generator; reusing it with a different one is a
# confusing CMake error, so catch it here with a useful message.
$cache = Join-Path $Build "CMakeCache.txt"
if ((Test-Path $cache) -and (-not $Clean)) {
    $prev = Get-Content $cache | Where-Object { $_ -like "CMAKE_GENERATOR:INTERNAL=*" } | Select-Object -First 1
    if ($prev) {
        $prev = $prev.Substring("CMAKE_GENERATOR:INTERNAL=".Length)
        if ($prev -ne $gen) {
            throw ("build\ was configured with generator '{0}' but this run wants '{1}'. Re-run with -Clean." -f $prev, $gen)
        }
    }
}

$configure = @("-S", $Root, "-B", $Build, "-G", $gen)

if ($gen -like "Visual Studio*") {
    $configure += @("-A", "x64")
} else {
    $configure += @("-DCMAKE_BUILD_TYPE=$Config")
}

if ($Effect) { $configure += "-DTONERIG_BUILD_AS_INSTRUMENT=OFF" }

# ------------------------------------------------------------- build --------
Write-Host ""
Write-Host "== Configuring ==" -ForegroundColor Cyan
Invoke-Native "cmake" $configure

Write-Host ""
Write-Host "== Building ($Config) ==" -ForegroundColor Cyan
Invoke-Native "cmake" @("--build", $Build, "--config", $Config,
                        "--target", "ToneRig_VST3", "ToneRigDspTest", "--parallel")

# ------------------------------------------------------------- tests --------
$testExe = Join-Path $Build "ToneRigDspTest_artefacts\$Config\ToneRigDspTest.exe"
if (-not (Test-Path $testExe)) {
    throw ("Expected the test binary at '{0}' but it is not there." -f $testExe)
}

Write-Host ""
Write-Host "== DSP chain tests ==" -ForegroundColor Cyan
Invoke-Native $testExe @()

# ----------------------------------------------------------- plugin ---------
$plugin = Join-Path $Build "ToneRig_artefacts\$Config\VST3\ToneRig.vst3"
if (-not (Test-Path $plugin)) {
    throw ("Build succeeded but no plug-in was produced at '{0}'." -f $plugin)
}

$vst3dir = if ($env:ProgramFiles) { Join-Path $env:ProgramFiles "Common Files\VST3" }
           else { "C:\Program Files\Common Files\VST3" }

if ($Install) {
    Write-Host ""
    Write-Host "== Installing ==" -ForegroundColor Cyan
    if (-not (Test-Path $vst3dir)) {
        New-Item -ItemType Directory -Force -Path $vst3dir | Out-Null
    }
    Copy-Item -Recurse -Force $plugin (Join-Path $vst3dir "ToneRig.vst3")
    Write-Host "Installed to $vst3dir\ToneRig.vst3"
}

Write-Host ""
Write-Host "=====================================================" -ForegroundColor Green
Write-Host " Built: $plugin"
if ($Install) {
    Write-Host " Installed into: $vst3dir"
    Write-Host " Next: in REAPER press F5, or Options > Preferences > Plug-ins > VST > Re-scan."
} else {
    Write-Host ""
    Write-Host " Install (needs admin for Program Files):"
    Write-Host ("   Copy-Item -Recurse -Force `"{0}`" `"{1}`"" -f $plugin, $vst3dir)
    Write-Host "   ...or just re-run this script with -Install from an admin shell."
    Write-Host ""
    Write-Host " Then in REAPER press F5, or Options > Preferences > Plug-ins > VST > Re-scan."
}
Write-Host "=====================================================" -ForegroundColor Green
