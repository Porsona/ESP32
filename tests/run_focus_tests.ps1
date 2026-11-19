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
$testExecutable = Join-Path $testBuild 'focus_services_tests.exe'
$sources = @('tests/focus_services_tests.cpp', 'src/services/ProgressService.cpp',
             'src/services/FocusModel.cpp', 'src/services/FocusSessionService.cpp')
Push-Location $projectRoot
$previousPath = $env:PATH
try {
    # MinGW cc1plus/collect2 also need the compiler's runtime DLL directory.
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $previousPath
    & $Compiler -std=c++11 -Wall -Wextra -Werror -pedantic -static -Isrc @sources -o $testExecutable
    if ($LASTEXITCODE -ne 0) { throw "Host compilation failed: $LASTEXITCODE" }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) { throw "Focus service tests failed: $LASTEXITCODE" }
    $uiExecutable = Join-Path $testBuild 'focus_ui_tests.exe'
    $uiSources = @('tests/focus_ui_tests.cpp', 'src/services/ProgressService.cpp',
                   'src/services/FocusModel.cpp', 'src/services/FocusSessionService.cpp',
                   'src/apps/FocusQuestScreen.cpp', 'src/core/AppManager.cpp', 'src/ui/PixelRenderer.cpp')
    & $Compiler -std=c++11 -Wall -Wextra -Werror -pedantic -static -Itests/fakes -Iinclude -Isrc @uiSources -o $uiExecutable
    if ($LASTEXITCODE -ne 0) { throw "UI host compilation failed: $LASTEXITCODE" }
    & $uiExecutable
    if ($LASTEXITCODE -ne 0) { throw "Focus UI integration tests failed: $LASTEXITCODE" }
} finally { $env:PATH = $previousPath; Pop-Location }
