# CYD DeskOS: Retro Pixel Starter v0.1

A **hardware-first** PlatformIO starter for common ESP32-2432S028R boards. This is a hardware test, not a completed operating system.

## Requirements

- VS Code with PlatformIO IDE extension
- ESP32-2432S028R (ILI9341 common version)
- Data-capable USB cable and correct Windows serial driver if required

## Run

1. Open this **folder** in VS Code.
2. Open PlatformIO Home; let packages download.
3. Connect board and identify COM port in Windows Device Manager.
4. PlatformIO: **Build**, then **Upload**, then **Monitor** at 115200.
5. Screen should show a Retro Pixel border and red/green/blue bars.
6. Tap the screen to cycle through DISPLAY CHECK, TOUCH CHECK, RETRO BOOT.
7. Capture Serial logs for touch raw x/y/z and report any misbehavior.

CLI alternatives:

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

## Important compatibility notes

- Many original single-USB models have ILI9341 panels; some newer models use ST7789. Do not assume your board uses ILI9341 if the display stays white. Change only driver first.
- Do not overwrite `TFT_eSPI/User_Setup.h` in dependency folders. The PlatformIO `build_flags` supply the configuration.
- Touch uses **separate SPI pins** (SCK 25, MISO 39, MOSI 32, CS 33, IRQ 36), whereas screen uses HSPI.
- The first test reports **raw touch coordinates**, not calibrated display coordinates.
- Other peripherals (microSD, RGB LED, sound, Wi-Fi) are intentionally excluded until base display/touch are confirmed.
- No firmware has been flashed or tested on your specific physical board by this starter archive's creator.

## Next milestones

- v0.2: `src/hal/Display.*`, `Touch.*`, `ui/ScreenManager.*`, `screens/ClockScreen.*`, `PetScreen.*`, `FocusScreen.*`, `SystemScreen.*`, `assets/`.
- v0.3: Add Wi-Fi config, NTP + timezone Asia/Ho_Chi_Minh, Open-Meteo with asynchronous fetch/retry.
- v0.4: Persist settings using Preferences; add Pomodoro, pixel animation.
- v0.5: Laptop telemetry API via local network, optional microSD assets.

## Sources

- https://github.com/HenrysCat/ESP32-2432S028-CYD-Diagnostic-Tool
- # https://github.com/boxwrench/DIST_ESP32-CYD-Tester/blob/main/docs/ESP32_CYD_REFERENCE.md

> > > > > > > origin/main
