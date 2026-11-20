param([string]$Compiler = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$Compiler) {
    $localCompiler = Join-Path $projectRoot '.pio/host-tools/toolchain-gccmingw32/bin/g++.exe'
    if (Test-Path -LiteralPath $localCompiler) { $Compiler = $localCompiler }
    else { $Compiler = (Get-Command g++ -ErrorAction Stop).Source }
}
$testBuild = Join-Path $PSScriptRoot '.build'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
$flags = @('-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-static')
$services = @('src/services/ProgressService.cpp', 'src/services/FocusModel.cpp',
              'src/services/FocusSessionService.cpp', 'src/services/MissionModel.cpp',
              'src/services/DailyMissionService.cpp')
Push-Location $projectRoot
$previousPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $previousPath
    # M2 business logic: production service sources, fake clock/storage, no waiting.
    $exe = Join-Path $testBuild 'mission_services_tests.exe'
    & $Compiler @flags -Isrc (@('tests/mission_services_tests.cpp') + $services) -o $exe
    if ($LASTEXITCODE -ne 0) { throw "Mission service compilation failed: $LASTEXITCODE" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Mission service tests failed: $LASTEXITCODE" }
    # M2 UI: real AppManager + Focus/Missions screens + renderer against a draw spy.
    $uiExe = Join-Path $testBuild 'mission_ui_tests.exe'
    $ui = @('tests/mission_ui_tests.cpp', 'src/apps/FocusQuestScreen.cpp', 'src/apps/DailyMissionsScreen.cpp',
            'src/core/AppManager.cpp', 'src/ui/PixelRenderer.cpp') + $services
    & $Compiler @flags -Itests/fakes -Iinclude -Isrc $ui -o $uiExe
    if ($LASTEXITCODE -ne 0) { throw "Mission UI compilation failed: $LASTEXITCODE" }
    & $uiExe
    if ($LASTEXITCODE -ne 0) { throw "Mission UI tests failed: $LASTEXITCODE" }
} finally { $env:PATH = $previousPath; Pop-Location }
# M1 regression suites (Focus timer, +25 XP, Focus UI) with the M2 sources.
& (Join-Path $PSScriptRoot 'run_focus_tests.ps1') -Compiler $Compiler
if ($LASTEXITCODE -ne 0) { throw "M1 regression tests failed: $LASTEXITCODE" }
