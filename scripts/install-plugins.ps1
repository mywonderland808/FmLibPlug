# Install FmLibPlug host formats on Windows (PowerShell).
# Usage (from repo root, after a Release or Debug CMake build):
#   powershell -ExecutionPolicy Bypass -File scripts/install-plugins.ps1
#   powershell -File scripts/install-plugins.ps1 -Config Release
#
# Override destinations:
#   -Vst3Dir "D:\Plugins\VST3" -ClapDir "D:\Plugins\CLAP" -Lv2Dir "D:\Plugins\LV2"

param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$Vst3Dir = "",
    [string]$ClapDir = "",
    [string]$Lv2Dir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ArtefactRoot = Join-Path $RepoRoot "$BuildDir\FmLibPlug_artefacts\$Config"

if (-not (Test-Path $ArtefactRoot)) {
    Write-Error "Artefact root missing: $ArtefactRoot`nBuild first, e.g. cmake --build $BuildDir --config $Config"
}

if ([string]::IsNullOrWhiteSpace($Vst3Dir)) {
    $Vst3Dir = Join-Path ${env:CommonProgramFiles} "VST3"
}
if ([string]::IsNullOrWhiteSpace($ClapDir)) {
    $ClapDir = Join-Path ${env:CommonProgramFiles} "CLAP"
}
if ([string]::IsNullOrWhiteSpace($Lv2Dir)) {
    $Lv2Dir = Join-Path $env:APPDATA "LV2"
}

function Copy-PluginTree([string]$Src, [string]$Dst) {
    if (-not (Test-Path $Src)) {
        Write-Warning "Missing artefact: $Src"
        return $false
    }
    $parent = Split-Path -Parent $Dst
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    if (Test-Path $Dst) { Remove-Item -Recurse -Force $Dst }
    Copy-Item -Recurse -Force $Src $Dst
    return $true
}

Write-Host ""
Write-Host "==== INSTALL (windows) ===="
Write-Host "Source:  $ArtefactRoot"
Write-Host "  VST3 -> $Vst3Dir\FmLibPlug.vst3"
Write-Host "  CLAP -> $ClapDir\FmLibPlug.clap"
Write-Host "  LV2  -> $Lv2Dir\FmLibPlug.lv2"
Write-Host "==============="

$ok = $true
if (-not (Copy-PluginTree (Join-Path $ArtefactRoot "VST3\FmLibPlug.vst3") (Join-Path $Vst3Dir "FmLibPlug.vst3"))) { $ok = $false }
if (-not (Copy-PluginTree (Join-Path $ArtefactRoot "CLAP\FmLibPlug.clap") (Join-Path $ClapDir "FmLibPlug.clap"))) { $ok = $false }
if (-not (Copy-PluginTree (Join-Path $ArtefactRoot "LV2\FmLibPlug.lv2") (Join-Path $Lv2Dir "FmLibPlug.lv2"))) { $ok = $false }

$stamp = Join-Path $ArtefactRoot "BUILD_STAMP.txt"
if (Test-Path $stamp) {
    Copy-Item -Force $stamp (Join-Path $Vst3Dir "FmLibPlug.vst3\BUILD_STAMP.txt") -ErrorAction SilentlyContinue
}

if (-not $ok) {
    Write-Error "Install incomplete — missing one or more artefacts."
}

Write-Host "Install complete. Quit the DAW fully before rescanning."
Write-Host "AU is Apple-only (not installed on Windows)."
$standalone = Join-Path $ArtefactRoot "Standalone\FmLibPlug.exe"
if (Test-Path $standalone) {
    Write-Host "Standalone (not installed): $standalone"
}
