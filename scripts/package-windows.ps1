param(
    [Parameter(Mandatory = $true)]
    [string]$AngleSource
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $true

# Run from an x64 VS2022 developer environment (the workflow calls VsDevCmd).
$repository = Split-Path -Parent $PSScriptRoot
$AngleSource = (Resolve-Path $AngleSource).Path
$build = Join-Path $AngleSource 'out/windows-x64'
$package = Join-Path $repository 'dist/ANGLE-windows-x64'
if (Test-Path $package) { Remove-Item -Recurse -Force $package }
$bin = Join-Path $package 'bin'
$lib = Join-Path $package 'lib'
$headers = Join-Path $package 'include'
$config = Join-Path $package 'build-config'
$tests = Join-Path $package 'tests'
New-Item -ItemType Directory -Force $bin, $lib, $headers, $config, $tests | Out-Null

foreach ($name in @('libEGL.dll', 'libGLESv2.dll', 'd3dcompiler_47.dll')) {
    Copy-Item (Join-Path $build $name) $bin
}
foreach ($name in @('libEGL', 'libGLESv2')) {
    # Chromium's Windows toolchain names import libraries <name>.dll.lib.
    Copy-Item (Join-Path $build "$name.dll.lib") (Join-Path $lib "$name.lib")
}
foreach ($name in @('EGL', 'GLES2', 'GLES3', 'KHR')) {
    Copy-Item -Recurse -Force (Join-Path $AngleSource "include/$name") $headers
}
Copy-Item (Join-Path $AngleSource 'LICENSE') $package
Copy-Item (Join-Path $repository 'PACKAGE_README.windows.md') (Join-Path $package 'README.md')
Copy-Item (Join-Path $build 'args.gn') (Join-Path $config 'args.windows.x64.gn')
Copy-Item (Join-Path $build 'args.full.txt') (Join-Path $config 'args.windows.x64.full.txt')
$smoke = Join-Path $tests 'WindowsEGLSmoke.cpp'
Copy-Item (Join-Path $repository 'tests/WindowsEGLSmoke.cpp') $smoke

# Verify the public ABI and architecture using the packaged DLLs.
foreach ($name in @('libEGL', 'libGLESv2')) {
    $dll = Join-Path $bin "$name.dll"
    $exports = & dumpbin /nologo /exports $dll
    $exports | Set-Content (Join-Path $config "$name.exports.txt")
    $expected = if ($name -eq 'libEGL') { 'eglGetProcAddress|eglCreateContext|eglSwapBuffers' } else { 'glClear|glCreateShader|glReadPixels' }
    foreach ($symbol in ($expected -split '\|')) {
        if (-not ($exports -match "\s$symbol\s*$")) { throw "$dll is missing $symbol" }
    }
    $pe = & dumpbin /nologo /headers $dll
    if (-not ($pe -match '8664 machine')) { throw "$dll is not x64" }
    & dumpbin /nologo /dependents $dll | Set-Content (Join-Path $config "$name.dependents.txt")
}

# Build and run against the staged package, not the ANGLE output directory.
Push-Location $build
try {
    & cl /nologo /std:c++17 /EHsc /MT /W4 "/I$headers" $smoke `
        "/Fo$build/angle-windows-smoke.obj" "/Fe$bin/angle-windows-smoke.exe" `
        (Join-Path $lib 'libEGL.lib') (Join-Path $lib 'libGLESv2.lib') user32.lib `
        /link /INCREMENTAL:NO
    if ($LASTEXITCODE -ne 0) { throw "Smoke test compilation failed: $LASTEXITCODE" }
}
finally { Pop-Location }
Push-Location $bin
try {
    & ./angle-windows-smoke.exe | Tee-Object (Join-Path $config 'smoke-test.txt')
    if ($LASTEXITCODE -ne 0) { throw "D3D11 WARP smoke test failed: $LASTEXITCODE" }
}
finally { Pop-Location }

$angleCommit = & git -C $AngleSource rev-parse HEAD
$packagingCommit = & git -C $repository rev-parse HEAD
$depotCommit = & git -C (Join-Path $env:RUNNER_TEMP 'angle-depot-tools') rev-parse HEAD
@(
    "ANGLE_REF=$env:ANGLE_REF"
    "ANGLE_COMMIT=$angleCommit"
    "PACKAGING_COMMIT=$packagingCommit"
    "DEPOT_TOOLS_COMMIT=$depotCommit"
    "RUN_ID=$env:GITHUB_RUN_ID"
    "VISUAL_STUDIO=$env:vs2022_install"
    "WINDOWS_SDK=$env:WindowsSDKVersion"
    'TARGET=win-x64'
    'CONFIGURATION=Release'
    'RENDERER=D3D11'
    'CRT=static (/MT; non-component Release)'
    'SOURCE_PATCHES=none (Apple-only patches are not applied)'
    'SMOKE_DEVICE=D3D11 WARP (software, not hardware GPU validation)'
) | Set-Content (Join-Path $config 'BUILD_INFO.windows.txt')

$manifest = Get-ChildItem $package -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($package, $_.FullName).Replace('\', '/')
    '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
}
$manifest | Set-Content (Join-Path $package 'SHA256SUMS')
$archive = Join-Path $repository 'ANGLE-windows-x64.zip'
Compress-Archive -Path $package -DestinationPath $archive -Force
Write-Host "Package ready: $archive"
