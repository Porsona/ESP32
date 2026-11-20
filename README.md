# CYD DeskOS — Retro Pixel desk companion (v1.0.0-rc1)

Firmware cho bo mạch **ESP32-2432S028R** ("Cheap Yellow Display", màn 2.8" 320×240
cảm ứng điện trở) biến CYD thành một "desk OS" phong cách Retro Pixel: hẹn giờ tập
trung, nhiệm vụ hằng ngày, thú cưng pixel lớn lên theo XP, cửa sổ pixel động, thời
tiết trực tiếp và bảng theo dõi PC — không dùng LVGL, chạy trên PlatformIO/Arduino.

> **Trạng thái thật (2026-11-20):** M0 và M1 đã được nghiệm thu trên thiết bị.
> M2–M6 và bản `v1.0.0-rc1` là **SOFTWARE PASS** (build + 80 nhóm test tự động) và
> **HARDWARE PENDING** — chưa được thử trên board. Xem
> [docs/DEVELOPMENT_STATUS.md](docs/DEVELOPMENT_STATUS.md) và
> [docs/FINAL_QA_REPORT.md](docs/FINAL_QA_REPORT.md).

## Sáu ứng dụng

| App | Chức năng | Cần mạng? |
| --- | --- | --- |
| **Focus Quest** | Pomodoro 25 phút / nghỉ 5 phút, +25 XP mỗi phiên hoàn thành, chạy nền qua mọi app | Không |
| **Daily Missions** | 5 nhiệm vụ mỗi ngày, +10 XP mỗi nhiệm vụ (không cộng trùng), NEW DAY, tự sang ngày lúc 00:00 khi có NTP | Không (NTP tùy chọn) |
| **Pixel Pet** | BIT lên level theo tổng XP, chạm để vuốt, ngủ khi để yên, style mở theo level | Không |
| **Pixel Window** | Cảnh DAY / NIGHT / RAIN / AUTO, tốc độ SLOW/NORMAL/FAST | AUTO cần NTP + thời tiết |
| **Living Weather** | Open-Meteo qua HTTPS đã xác thực, cập nhật 15 phút, cache khi offline | Có |
| **PC Monitor** | CPU/RAM/DISK/mạng/pin từ laptop Windows qua UDP LAN có HMAC | Có (LAN) |

XP dùng chung: Focus + Missions; Pet chỉ đọc XP. Mất Wi-Fi thì các app offline vẫn
chạy bình thường.

## Phần cứng và phần mềm cần có

- ESP32-2432S028R bản **ILI9341** (một cổng USB), cáp USB dữ liệu, nguồn USB đủ dòng
  (Wi-Fi tăng dòng tiêu thụ).
- Windows 11 + VS Code + PlatformIO IDE (hoặc PlatformIO Core).
- Wi-Fi 2.4 GHz cho Weather/NTP/PC Monitor.
- Python 3.8+ trên PC cho PC Agent (không cần thư viện ngoài; `psutil` tùy chọn).

Cấu hình đã nghiệm thu, **không đổi**: `ILI9341_2_DRIVER`, TFT trên HSPI (MISO 12,
MOSI 13, SCLK 14, CS 15, DC 2, BL 21), touch XPT2046 trên SPI riêng (SCK 25, MISO 39,
MOSI 32, CS 33, IRQ 36), `setRotation(1)`, `invertDisplay(true)`, `LOAD_GLCD=1`,
TFT_eSPI 2.5.43, XPT2046_Touchscreen v1.4, espressif32@6.4.0 (Arduino-ESP32 2.0.11),
partition mặc định.

## Build và Upload

```powershell
pio run -e cyd_ili9341                                  # build
pio run -e cyd_ili9341 -t upload --upload-port COM5     # nạp (KHÔNG xóa NVS)
pio device monitor --port COM5 --baud 115200            # Serial
```

Nếu `pio` không có trong PATH: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" …`.
Nạp thường giữ nguyên calibration, XP, quest, pet, window, weather, Wi-Fi và token PC.
**Không** dùng "Erase Flash".

Kiểm thử tự động trên máy (không cần board):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_all_tests.ps1
```

## Cấu hình Wi-Fi

Trong Serial Monitor (115200, kết thúc bằng newline):

```text
WIFI SSID TenWifiCuaBan
WIFI PASS matkhau-wifi
WIFI
```

Mật khẩu chỉ lưu trong NVS của ESP32 và không bao giờ được in lại. `WIFI OFF` /
`WIFI ON` / `WIFI FORGET` để tắt, bật, xóa. Danh sách lệnh đầy đủ:
[docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Chạy PC Agent

1. Serial: `PC TOKEN` → ESP32 in token và lệnh mẫu.
2. Trên PC:

   ```powershell
   python tools/pc_agent/deskos_agent.py --host <IP ESP32> --token <TOKEN> --save-config
   ```

3. Mở app PC MONITOR → "CONNECTED". Agent chỉ gửi (không mở cổng, không nhận lệnh),
   chỉ gửi trong LAN. Chi tiết và xử lý sự cố: [docs/PC_AGENT_SETUP.md](docs/PC_AGENT_SETUP.md).

## Cấu trúc thư mục

```text
platformio.ini            Cấu hình board/TFT (đã nghiệm thu, không đổi)
include/                  board_pins.h, DisplayHAL.h, DiagnosticScreen.h
src/main.cpp              Nối các module, vòng lặp không delay, lệnh Serial
src/core/                 AppManager, kiểu app, calibration cảm ứng
src/hal/                  TouchHAL (XPT2046), FocusClock
src/apps/                 Home + 6 app + màn CAL/TEST
src/services/             Logic thuần đã test: XP, Focus, Missions, Pet, Window, Weather, PC
src/storage/              Adapter NVS (ghi + đọc lại kiểm tra)
src/net/                  Wi-Fi, NTP, HTTPS task, UDP PC link, root CA
src/ui/                   PixelRenderer, theme, sprite, animator
tests/                    Test host C++11 (+ fakes) và runner PowerShell
tools/pc_agent/           PC Agent Python + test
tools/hw_validation/      Logger Serial có che secrets + phân tích log
docs/                     Kiến trúc, trạng thái, kế hoạch test phần cứng, báo cáo
CLAUDE.md                 Quy tắc bảo trì cho Claude Code
```

## Tiến độ thực tế

| Milestone | Software | Hardware |
| --- | --- | --- |
| M0 Shell / Touch / Diag | PASS | **PASS** (người dùng) |
| M1 Focus Quest | PASS | **PASS** (người dùng) |
| M2 Daily Missions | PASS | PENDING |
| M3 Pixel Pet | PASS | PENDING |
| M4 Pixel Window | PASS | PENDING |
| M5 Wi-Fi / NTP / Living Weather | PASS | PENDING |
| M6 PC Monitor | PASS | PENDING |
| v1.0.0-rc1 hardening | PASS | PENDING |

Kế hoạch nghiệm thu: [docs/HARDWARE_TEST_PLAN.md](docs/HARDWARE_TEST_PLAN.md).
Ghi chú phát hành: [docs/RELEASE_NOTES.md](docs/RELEASE_NOTES.md).

## Giới hạn hiện tại

Xem đầy đủ trong [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md). Tóm tắt:

- Chưa nghiệm thu M2–M6 trên board; heap runtime khi Wi-Fi + TLS chưa đo.
- Kiểm hạn chứng chỉ chỉ áp dụng cho chứng chỉ leaf và chỉ khi đã có giờ NTP
  (mbedTLS của framework không tự kiểm ngày).
- PC Monitor xác thực nhưng không mã hóa (người cùng LAN đọc được số liệu CPU/RAM).
- Chỉ hỗ trợ màn ILI9341; bản ST7789 cần đổi driver và nghiệm thu lại.
- Giờ địa phương cố định UTC+7 (Asia/Ho_Chi_Minh).

## Nguồn tham khảo

- https://github.com/HenrysCat/ESP32-2432S028-CYD-Diagnostic-Tool
- https://github.com/boxwrench/DIST_ESP32-CYD-Tester/blob/main/docs/ESP32_CYD_REFERENCE.md
- Dữ liệu thời tiết: [Open-Meteo](https://open-meteo.com/) (CC BY 4.0)
