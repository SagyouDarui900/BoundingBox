$ErrorActionPreference = "Stop"

$distDir = Join-Path $PSScriptRoot "dist"
$zipPath = Join-Path $distDir "BoundingBox.zip"

if (Test-Path $distDir) {
    Remove-Item -LiteralPath $distDir -Recurse -Force
}
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

# Ensure BoundingBox.mod2 is built
if (-not (Test-Path (Join-Path $PSScriptRoot "BoundingBox.mod2"))) {
    Write-Host "BoundingBox.mod2 not found. Building first..."
    & (Join-Path $PSScriptRoot "build-BoundingBox.ps1")
}

$files = @(
    (Join-Path $PSScriptRoot "BoundingBox.anm2"),
    (Join-Path $PSScriptRoot "BoundingBox.mod2"),
    (Join-Path $PSScriptRoot "README.md"),
    (Join-Path $PSScriptRoot "LICENSE")
)

Compress-Archive -Path $files -DestinationPath $zipPath -Force
Write-Host "Created distribution package: $zipPath"
