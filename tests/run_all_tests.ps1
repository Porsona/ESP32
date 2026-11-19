param([string]$Compiler = '', [switch]$SkipLegacy)
# Runs every host suite: M1 Focus + M2 Missions (legacy runners), then M3+ suites.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$Compiler) {
    $localCompiler = Join-Path $projectRoot '.pio/host-tools/toolchain-gccmingw32/bin/g++.exe'
    if (Test-Path -LiteralPath $localCompiler) { $Compiler = $localCompiler }
    else { $Compiler = (Get-Command g++ -ErrorAction Stop).Source }
}
if (!$SkipLegacy) {
    & (Join-Path $PSScriptRoot 'run_mission_tests.ps1') -Compiler $Compiler
    if ($LASTEXITCODE -ne 0) { throw "M1/M2 suites failed: $LASTEXITCODE" }
}
$testBuild = Join-Path $PSScriptRoot '.build'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
$flags = @('-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-static', '-Itests/fakes', '-Iinclude', '-Isrc')
$core = @('src/services/ProgressService.cpp', 'src/core/AppManager.cpp', 'src/ui/PixelRenderer.cpp')
$suites = [ordered]@{
    'pet_tests' = @('tests/pet_tests.cpp', 'src/services/PetModel.cpp', 'src/services/PetService.cpp',
                    'src/ui/PetSprites.cpp', 'src/apps/PixelPetScreen.cpp')
}
Push-Location $projectRoot
$previousPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $previousPath
    foreach ($name in $suites.Keys) {
        $exe = Join-Path $testBuild "$name.exe"
        & $Compiler @flags ($suites[$name] + $core) -o $exe
        if ($LASTEXITCODE -ne 0) { throw "$name compilation failed: $LASTEXITCODE" }
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "$name failed: $LASTEXITCODE" }
    }
} finally { $env:PATH = $previousPath; Pop-Location }
