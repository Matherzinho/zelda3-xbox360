$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $PSScriptRoot 'build'
$packageDir = Join-Path $buildDir 'usb-package'
$xexSource = Join-Path $buildDir 'zelda3_xbox360.xex'
$assetsSource = Join-Path $projectRoot 'zelda3_assets.dat'

if (-not (Test-Path -LiteralPath $xexSource -PathType Leaf)) {
    Write-Error "Missing Xbox executable: $xexSource"
    exit 1
}
if ((Get-Item -LiteralPath $xexSource).Length -eq 0) {
    Write-Error "Xbox executable is empty: $xexSource"
    exit 1
}
if (-not (Test-Path -LiteralPath $assetsSource -PathType Leaf)) {
    Write-Error "Missing required assets: $assetsSource`nGenerate this file from a legally owned compatible ROM; see xbox360/README.md. No package files were copied."
    exit 1
}

New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
Copy-Item -LiteralPath $xexSource -Destination (Join-Path $packageDir 'zelda3_xbox360.xex') -Force
Copy-Item -LiteralPath $assetsSource -Destination (Join-Path $packageDir 'zelda3_assets.dat') -Force
Get-Item -LiteralPath (Join-Path $packageDir 'zelda3_xbox360.xex'), (Join-Path $packageDir 'zelda3_assets.dat') |
    Select-Object FullName, Length | Format-Table -AutoSize
Write-Host "Package ready: $packageDir"
