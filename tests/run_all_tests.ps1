param([string]$Compiler = '', [string]$Python = 'python', [switch]$SkipLegacy)
# Runs every host suite: M1 Focus + M2 Missions (legacy runners), M3-M6 C++ suites
# and the Python PC agent tests (including a firmware <-> agent HMAC cross-check).
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
    'window_tests' = @('tests/window_tests.cpp', 'src/services/WindowSettings.cpp', 'src/ui/WindowAnimator.cpp',
                       'src/apps/PixelWindowScreen.cpp')
    'weather_tests' = @('tests/weather_tests.cpp', 'src/services/WeatherModel.cpp', 'src/services/WeatherService.cpp',
                        'src/ui/WeatherSprites.cpp', 'src/apps/WeatherScreen.cpp', 'src/services/NetTypes.cpp')
    'pc_tests' = @('tests/pc_tests.cpp', 'src/services/Sha256.cpp', 'src/services/PcMonitorModel.cpp',
                   'src/services/PcMonitorService.cpp', 'src/apps/PcMonitorScreen.cpp', 'src/services/NetTypes.cpp')
    'shell_tests' = @('tests/shell_tests.cpp', 'src/apps/HomeScreen.cpp', 'src/apps/FocusQuestScreen.cpp',
                      'src/apps/DailyMissionsScreen.cpp', 'src/apps/PixelPetScreen.cpp', 'src/apps/PixelWindowScreen.cpp',
                      'src/apps/WeatherScreen.cpp', 'src/apps/PcMonitorScreen.cpp', 'src/services/FocusModel.cpp',
                      'src/services/FocusSessionService.cpp', 'src/services/MissionModel.cpp',
                      'src/services/DailyMissionService.cpp', 'src/services/PetModel.cpp', 'src/services/PetService.cpp',
                      'src/services/WindowSettings.cpp', 'src/services/WeatherModel.cpp', 'src/services/WeatherService.cpp',
                      'src/services/PcMonitorModel.cpp', 'src/services/PcMonitorService.cpp', 'src/services/Sha256.cpp',
                      'src/ui/PetSprites.cpp', 'src/ui/WindowAnimator.cpp', 'src/ui/WeatherSprites.cpp', 'src/services/NetTypes.cpp')
}
Push-Location $projectRoot
$previousPath = $env:PATH
try {
    # Packet produced by the real agent code for the cross-language HMAC check.
    $agentPacket = Join-Path $testBuild 'agent_packet.txt'
    $packet = & $Python -I tools/pc_agent/deskos_agent.py --emit-test-packet --token 00112233445566778899aabbccddeeff
    if ($LASTEXITCODE -ne 0) { throw "Python agent packet generation failed: $LASTEXITCODE" }
    [System.IO.File]::WriteAllText($agentPacket, ($packet -join ''))
    $suiteArgs = @{ 'pc_tests' = @($agentPacket) }
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $previousPath
    foreach ($name in $suites.Keys) {
        $exe = Join-Path $testBuild "$name.exe"
        & $Compiler @flags ($suites[$name] + $core) -o $exe
        if ($LASTEXITCODE -ne 0) { throw "$name compilation failed: $LASTEXITCODE" }
        $extra = @(); if ($suiteArgs.ContainsKey($name)) { $extra = $suiteArgs[$name] }
        & $exe @extra
        if ($LASTEXITCODE -ne 0) { throw "$name failed: $LASTEXITCODE" }
    }
    $env:PATH = $previousPath
    & $Python -I tools/pc_agent/test_deskos_agent.py
    if ($LASTEXITCODE -ne 0) { throw "Python agent tests failed: $LASTEXITCODE" }
    & $Python -I tools/hw_validation/test_deskos_serial_log.py
    if ($LASTEXITCODE -ne 0) { throw "Serial log tool tests failed: $LASTEXITCODE" }
} finally { $env:PATH = $previousPath; Pop-Location }
