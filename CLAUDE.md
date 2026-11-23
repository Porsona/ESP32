# CLAUDE.md — CYD DeskOS maintenance guide

Firmware for the ESP32-2432S028R ("Cheap Yellow Display"): a Retro Pixel desk
companion with six apps (Focus Quest, Daily Missions, Pixel Pet, Pixel Window,
Living Weather, PC Monitor). PlatformIO + Arduino-ESP32 2.0.11, no LVGL.

The owner writes specs in Vietnamese and expects reports in Vietnamese.
Status and evidence live in `docs/DEVELOPMENT_STATUS.md` and `docs/FINAL_QA_REPORT.md`;
read them (and `git log`) before starting any task — features already exist, do not
re-implement a milestone because a prompt describes it again.

## Hard rules (never break without explicit permission in the current request)

- Never upload firmware, erase flash, erase/clear NVS or run `-t erase`. The owner
  flashes the board (COM5) and runs hardware tests.
- Never mark a hardware test PASS without the owner's log/photo. Use
  SOFTWARE PASS / HARDWARE PASS / HARDWARE PENDING / FAIL exactly.
- Never push, force-push, create remote tags or GitHub releases. Local commits and
  local annotated tags are fine after reviewing `git diff`.
- Do not change hardware configuration that passed on the board: `platformio.ini`
  build flags (ILI9341_2_DRIVER, pins, HSPI, SPI speeds, LOAD_GLCD=1),
  `include/board_pins.h`, `DisplayHAL` (setRotation(1), invertDisplay(true), init
  order `display.begin()` → `touch.begin()` → `applyColorConfig()`), `src/hal/TouchHAL.*`,
  touch calibration (`src/core/TouchCalibration*`, NVS `cyd-touch`). Verify with
  `git diff m1-stable -- platformio.ini include src/hal src/core/TouchCalibration*`.
- M1 Focus is hardware-accepted: `src/services/Focus*`, `src/storage/FocusStorageNVS.*`,
  `src/apps/FocusQuestScreen.*` change only for a proven bug.
- No secrets in Git: Wi-Fi credentials live only in ESP32 NVS (`deskos-net`), the PC
  token only in NVS (`deskos-pc`) and `%APPDATA%\DeskOS\agent.json`. Never commit
  Serial logs that contain a token; use the redacting logger (below).
- TLS verification stays on: never `setInsecure()`, never drop the CA bundle.
- Do not change the partition table without measured evidence (app slot 1,310,720 B).

## Build and test

```powershell
# Firmware (pio is not on PATH; use the PlatformIO venv)
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e cyd_ili9341
# Every host suite: C++ (GCC 5.1, C++11, -Wall -Wextra -Werror -pedantic) + Python
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_all_tests.ps1
```

- Host compiler: `.pio/host-tools/toolchain-gccmingw32/bin/g++.exe` (C++11 only —
  no `std::optional`, no C++14 features). Fakes in `tests/fakes/` (Arduino, TFT draw spy
  that counts out-of-bounds draws and measures GLCD text width).
- Add new source files to the suite lists in `tests/run_all_tests.ps1`; M1/M2 have
  their own runners (`run_focus_tests.ps1`, `run_mission_tests.ps1`).
- Expected warnings: only TFT_eSPI's `TOUCH_CS pin not defined`. Zero warnings from `src/`.
- After changing a guarded behaviour, do a quick mutation check (break the code,
  confirm the new test fails, restore).
- Windows Git Bash: editing C files through a Python heredoc turned `\n` inside
  string literals into real newlines. Use the Edit tool for C/C++ string changes.

## Architecture in one screen

- `src/main.cpp`: globals wiring, Serial command parser (bounded, 112-byte line),
  `loop()` without `delay()`: Focus → Network → Missions → Weather → PcLink → Serial →
  touch → view/AppManager. Services run on every screen; only the visible app renders.
- `src/core/AppManager`: registry Home=0, Pet=1, Window=2, Focus=3, Missions=4,
  PcMonitor=5, Weather=6; deferred `open()`, tap = press+release without drag/hold.
- `src/services/`: pure, host-tested logic (models, records with magic/version/size/CRC32).
  `ProgressService` is the single shared XP: Focus +25 per session (inside the M1
  Focus record) + mission ledger +10 per quest (inside the Missions A/B record);
  reward key `mission:<day>:<quest>` is idempotent. Pet only reads XP.
- `src/storage/`: NVS adapters (write + independent read-back). `BlobStoreNVS` serves
  Pet/Window/Weather/PC records.
- `src/net/`: `NetworkService` (Wi-Fi STA, non-blocking, backoff), `TimeServiceESP`
  (SNTP, UTC+7, valid only after a sync this boot), `HttpsFetchTask` (one FreeRTOS
  task, core 0, priority 0, fixed buffers; CA chain + hostname + leaf date check),
  `PcLinkUDP` (receive-only UDP 47801).
- `src/ui/`: `PixelRenderer` (GLCD font, panels, sprites drawn without framebuffer),
  `UITheme` (palette + layout rects: content y=32..199, BACK/HOME y=200..239).
- `tools/pc_agent/deskos_agent.py`: send-only, LAN-only telemetry agent (stdlib only).
- `tools/hw_validation/deskos_serial_log.py`: redacting Serial logger + log analyzer.

## NVS namespaces (never erase; each record validated independently)

`cyd-touch` (calibration), `deskos-focus`, `deskos-quest` (slot0/slot1 A/B),
`deskos-pet`, `deskos-win`, `deskos-wthr` (loc, cache), `deskos-net` (ssid/pass/en),
`deskos-pc` (token, ctr = replay floor). Change a record layout ⇒ bump its schema
version and keep a `static_assert(sizeof(...))`. Writes must stay rare (actions,
app exit, ≥60 s Focus checkpoint, ≥15 min weather cache / PC replay floor).

## UI conventions

Palette Background #171329, Panel #43355D, Mint #72E6BA, Gold #F3D37A, Rose #F28FAB.
GLCD font: 6×8 px per char × text size. Every app: full clear only on enter, then
dirty-region redraws; no heap allocation in update/render/touch (tests count
`operator new`). Hitboxes ≥40 px for navigation. Check text fits: width =
chars × 6 × size.

## Workflow

1. Audit: `git status`, `git log --oneline -15`, `git tag`, read the status docs.
2. Plan small changes; keep diffs minimal; no new classes when an existing module fits.
3. Build after each compilable step; run the full suite; add regression tests.
4. Update docs (`DEVELOPMENT_STATUS.md`, `KNOWN_ISSUES.md`, `HARDWARE_TEST_PLAN.md`,
   feature docs, `RELEASE_NOTES.md`) with measured numbers only — never invent logs.
5. Stop before Upload; give the owner the exact upload + hardware test steps.
6. When the owner sends logs: diagnose from the log, fix, rebuild, give regression steps.

Release tags: `m0-stable`, `m1-stable` (hardware-accepted), `v1.0.0-rc1` (software
verified, hardware pending). `v1.0.0` only after the owner confirms hardware acceptance.
