#Requires -Version 7
<#
.SYNOPSIS
Builds kokoro.dll per RID against a pinned ONNX Runtime and stages it into artifacts/natives/<rid>/.
.PARAMETER Rid
RIDs to build (default: every row of packaging/rids.psd1).
#>
param([string[]]$Rid)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Kokoro.Packaging.psm1') -Force

$root = Split-Path $PSScriptRoot -Parent

function Select-Rows([object[]]$Rows, [string[]]$Requested) {
    if (-not $Requested) { return $Rows }
    foreach ($name in $Requested) {
        if ($name -notin $Rows.Rid) { throw "Unknown RID $name; known: $($Rows.Rid -join ', ')" }
    }
    $Rows | Where-Object { $_.Rid -in $Requested }
}

function Get-OrtDir([hashtable]$Row) {
    Join-Path $root "build/packaging/ort/$($Row.Rid)"
}

function Get-BuildDir([hashtable]$Row) {
    Join-Path $root "build/packaging/$($Row.Rid)"
}

function Invoke-OrtFetch([hashtable]$Row) {
    $fetchArgs = @('run', 'bootstrap.py', 'fetch-ort', '--platform', $Row.OrtPlatform, '--arch', $Row.OrtArch,
        '--dest', "build/packaging/ort/$($Row.Rid)")
    Push-Location $root
    try { Invoke-Checked uv $fetchArgs } finally { Pop-Location }
}

function Invoke-NativeBuild([hashtable]$Row) {
    $buildDir = Get-BuildDir $Row
    $ortRoot = (Get-OrtDir $Row).Replace('\', '/')
    Invoke-InVcEnvironment $Row.VcvarsArch {
        if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
            throw 'ninja not found on PATH (install the VS "C++ CMake tools" component or Ninja)'
        }
        Invoke-Checked cmake @('-S', $root, '-B', $buildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
            "-DONNXRUNTIME_ROOT=$ortRoot", '-DKOKORO_BUILD_EVAL=OFF', '-DKOKORO_BUILD_SERVER=OFF')
        Invoke-Checked cmake @('--build', $buildDir, '--parallel')
    }
}

function Assert-NativeMachine([hashtable]$Row) {
    foreach ($file in $Row.Packages.Files) {
        $machine = Get-PeMachine (Join-Path (Get-BuildDir $Row) $file)
        if ($machine -ne $Row.PeMachine) {
            throw ('{0} is machine 0x{1:X}, {2} needs 0x{3:X}' -f $file, $machine, $Row.Rid, $Row.PeMachine)
        }
    }
}

function Copy-Checked([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { throw "Missing build output: $Source" }
    Copy-Item -LiteralPath $Source -Destination $Destination
}

function Copy-StagedNatives([hashtable]$Row) {
    $stage = Join-Path $root "artifacts/natives/$($Row.Rid)"
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory -Path $stage | Out-Null
    foreach ($file in $Row.Packages.Files) { Copy-Checked (Join-Path (Get-BuildDir $Row) $file) $stage }
    Write-Host "Staged $($Row.Rid) natives in $stage"
}

foreach ($row in Select-Rows (Get-KokoroRids) $Rid) {
    Invoke-OrtFetch $row
    Invoke-NativeBuild $row
    Assert-NativeMachine $row
    Copy-StagedNatives $row
}
