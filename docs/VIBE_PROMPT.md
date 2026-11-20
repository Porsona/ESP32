# Prompt for Coding Agent

You are an embedded C++ engineer and Retro Pixel UI designer. Work inside this PlatformIO Arduino project targeting ESP32-2432S028R. Read README.md and platformio.ini before editing. Do not replace working board pin mappings on a guess.

Task: Build DeskOS v0.2 with a 320x240 landscape 8-bit retro UI, 4 navigable screens (Clock, Pixel Pet, Focus, System), a fixed bottom pixel navigation, and separated DisplayHAL / TouchHAL / UI / Screen modules. Use a small static memory footprint, no massive full-screen canvas. Touch XPT2046 is on its own SPI bus (CLK25, MISO39, MOSI32, CS33, IRQ36). TFT screen uses configured TFT_eSPI via PlatformIO build_flags. Implement calibration as a separate, explicit step; do not claim raw coordinates are pixel coordinates.

Work incrementally:
1. Explain files to change and known hardware assumptions.
2. Preserve first working screen/test as a recovery mode.
3. Implement and compile after each component.
4. Provide commands for build, upload, monitor.
5. Do not add Wi-Fi, weather, Spotify, music streaming or laptop telemetry in v0.2.
6. If you cannot run `pio` or access hardware, state exactly what was and wasn't verified.

Acceptance: no blocking redraw loop; each screen responds to taps; no flickering at idle; no mixed responsibilities; code builds locally under `pio run` if the toolchain is available; no secrets checked into the repository.
