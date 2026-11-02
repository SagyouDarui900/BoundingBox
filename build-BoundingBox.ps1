[CmdletBinding()]
param (
    [string]$SdkPath = ""
)

$ErrorActionPreference = "Stop"

# 1. Detect or download SDK
$sdk = $SdkPath

if (-not $sdk -or -not (Test-Path (Join-Path $sdk "filter2.h"))) {
    $candidates = @(
        (Join-Path $PSScriptRoot "aviutl2_sdk"),
        (Join-Path $PSScriptRoot "include\aviutl2_sdk"),
        "C:\Users\tsuru\Videos\AviUtl2_開発用\.agents\skills\AviUtl2\reference\aviutl2_sdk_mirror-main\include\aviutl2_sdk"
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c "filter2.h"))) {
            $sdk = $c
            break
        }
    }
}

if (-not $sdk -or -not (Test-Path (Join-Path $sdk "filter2.h"))) {
    $found = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE "Documents") -Directory -Recurse -Filter "aviutl2_sdk" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $sdk = $found.FullName
        if (-not (Test-Path (Join-Path $sdk "filter2.h")) -and (Test-Path (Join-Path $sdk "include\aviutl2_sdk\filter2.h"))) {
            $sdk = Join-Path $sdk "include\aviutl2_sdk"
        }
    }
}

if (-not $sdk -or -not (Test-Path (Join-Path $sdk "filter2.h"))) {
    Write-Host "aviutl2_sdk was not found locally. Cloning aviutl2_sdk from GitHub..."
    $sdkDir = Join-Path $PSScriptRoot "external\aviutl2_sdk"
    if (-not (Test-Path $sdkDir)) {
        git clone --depth 1 https://github.com/kenkun0113/aviutl2_sdk.git $sdkDir
    }
    if (Test-Path (Join-Path $sdkDir "include\aviutl2_sdk\filter2.h")) {
        $sdk = Join-Path $sdkDir "include\aviutl2_sdk"
    } elseif (Test-Path (Join-Path $sdkDir "filter2.h")) {
        $sdk = $sdkDir
    }
}

if (-not $sdk -or -not (Test-Path (Join-Path $sdk "filter2.h"))) {
    throw "aviutl2_sdk (filter2.h) was not found. Please specify -SdkPath <path_to_sdk>"
}

Write-Host "Using SDK: $sdk"

# 2. Detect vcvarsall.bat
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null

if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $candidate) {
            $vcvars = $candidate
        }
    }
}

if (-not $vcvars) {
    $searchPaths = @(
        "C:\Program Files\Microsoft Visual Studio",
        "C:\Program Files (x86)\Microsoft Visual Studio"
    )
    foreach ($sp in $searchPaths) {
        if (Test-Path $sp) {
            $found = Get-ChildItem -Path $sp -Recurse -Filter "vcvarsall.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) {
                $vcvars = $found.FullName
                break
            }
        }
    }
}

if (-not $vcvars) {
    throw "Visual Studio / MSVC environment (vcvarsall.bat) was not found."
}

Write-Host "Using VC environment: $vcvars"

# 3. Build BoundingBox.mod2
$source = Join-Path $PSScriptRoot "BoundingBox.cpp"
$output = Join-Path $PSScriptRoot "BoundingBox.mod2"
$build = Join-Path $env:TEMP "BoundingBoxBuild"

New-Item -ItemType Directory -Path $build -Force | Out-Null

try {
    $cmd = """$vcvars"" x64 && cl.exe /nologo /O2 /std:c++17 /utf-8 /EHsc /LD ""/I$sdk"" ""/Fo$build\BoundingBox.obj"" ""$source"" /link ""/OUT:$build\BoundingBox.mod2"" ""/IMPLIB:$build\BoundingBox.lib"""
    cmd.exe /c $cmd

    if ($LASTEXITCODE -ne 0) {
        throw "BoundingBox.mod2 build failed with exit code $LASTEXITCODE."
    }

    Copy-Item -LiteralPath "$build\BoundingBox.mod2" -Destination $output -Force
    Write-Host "Successfully built and copied to $output"
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force -ErrorAction SilentlyContinue
}
