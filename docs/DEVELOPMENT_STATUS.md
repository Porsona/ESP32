# CYD DeskOS — Development Status

Cập nhật 2026-11-20. Branch `feature/m2-daily-missions` (M2→M6 nối tiếp, mỗi
milestone một commit checkpoint). Không push, không upload, không erase NVS.

Quy ước: **SOFTWARE PASS** = đã kiểm bằng build + test host; **HARDWARE PENDING**
= cần người dùng thử trên ESP32; **FAIL** = lỗi chưa giải quyết (hiện không có).

## Tổng quan

| Milestone | Firmware | Commit | Software | Hardware |
| --- | --- | --- | --- | --- |
| M0 Shell/Touch/Diag | M0_SHELL_01 | `654b069` | PASS | PASS (người dùng) |
| M1 Focus Quest | M1_FOCUS_01 | `c59cfae` | PASS 21/21 | PASS (người dùng, 10/10) |
| M2 Daily Missions | M2_MISSIONS_01 | `70099fc` | PASS 17/17 | PENDING |
| M3 Pixel Pet | M3_PET_01 | `592f658` | PASS 5/5 | PENDING |
| M4 Pixel Window | M4_WINDOW_01 | `200f4ab` | PASS 5/5 | PENDING |
| M5 Living Weather | M5_WEATHER_01 | `80d0ebd` | PASS 6/6 | PENDING |
| M6 PC Monitor | M6_PCMON_01 | commit cuối | PASS 5/5 C++ + 9/9 Python | PENDING |
| Tích hợp shell | M6_PCMON_01 | commit cuối | PASS 3/3 | PENDING |

Ghi chú: TEST_REPORT_M1.md vẫn ghi T01–T12 PENDING vì log nghiệm thu M1 chưa được
đưa vào repo; trạng thái PASS của M1 dựa trên xác nhận của người dùng.

## Build từng milestone (PlatformIO `cyd_ili9341`, espressif32 6.4.0)

| Mốc | Static RAM | Flash | Log |
| --- | --- | --- | --- |
| M1 (baseline) | 23,272 B (7.1%) | 323,181 B (24.7%) | M1_focus_build.log |
| M2 | 23,824 B (7.3%) | 334,861 B (25.5%) | M2_missions_build.log |
| M3 | 23,952 B (7.3%) | 342,025 B (26.1%) | M3_pet_build.log |
| M4 | 24,464 B (7.5%) | 347,641 B (26.5%) | M4_window_build.log |
| M5 | 51,892 B (15.8%) | 1,025,353 B (78.2%) | M5_weather_build.log |
| M6 cuối (clean build) | 52,868 B (16.1%) | 1,038,233 B (79.2%) | M6_final_clean.log, M6_final_build.log |

Bước nhảy ở M5 là Wi-Fi + lwIP + mbedTLS + HTTPClient (+~680 KB Flash, ~27 KB RAM
tĩnh). firmware.bin cuối: 1,043,984 B, SHA256
`41211AAB26695D850F641052B198B73401821A550E858B0C2A58FCF55B0506C1`.
Chỉ còn warning `TOUCH_CS pin not defined` của TFT_eSPI như baseline; 0 warning từ `src/`.
Không cần đổi partition table (còn 272 KB).

## Automated tests (host GCC 5.1, C++11, `-Wall -Wextra -Werror -pedantic`)

Chạy tất cả: `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_all_tests.ps1`
(log cuối: `docs/M6_final_tests.log`). Tổng **71 nhóm test PASS** (62 C++ + 9 Python).

| Suite | Nhóm | Nội dung chính |
| --- | --- | --- |
| M1 focus_services / focus_ui | 16 + 5 | Timer 25/5, +25 XP, checkpoint, restore, UI |
| M2 mission_services / mission_ui | 12 + 5 | Quest, +10 XP idempotent, NVS A/B, reboot, FocusCompleted, NEW DAY |
| M3 pet_tests | 5 | Level/unlock, mood, TAP, lưu khi rời app, record hỏng, 0 cấp phát |
| M4 window_tests | 5 | Scene/speed, AUTO không đoán giờ, 20k bước/scene không vẽ đè khung, 0 cấp phát |
| M5 weather_tests | 6 | Parser (mẫu API thật), timeout/backoff/watchdog, cache/offline, đổi vị trí |
| M6 pc_tests | 5 | SHA-256/HMAC vector chuẩn, DPM1, replay/subnet/flood, timeout, kiểm chéo với agent Python |
| shell_tests | 3 | 6 app mở từ Home, HOME/BACK, timer Focus qua mọi app, XP dùng chung, 0 cấp phát |
| tools/pc_agent | 9 | HMAC, clamp, LAN-only, counter, send-only (không bind/listen/exec) |

Lỗi thật tìm được và đã sửa nhờ test: chạm lặp sau CONFIRM tick nhầm quest (M2,
thêm guard 500 ms); kết quả thời tiết của vị trí cũ bị gán cho vị trí mới khi đổi
vị trí lúc đang fetch (M5); cờ discard không được xóa khi watchdog timeout (M5).

## Đã xác minh bằng công cụ

- Tất cả 6 app mở từ Home; HOME/BACK hoạt động (shell_tests, từng suite).
- Focus không đổi (`git diff m1-stable` rỗng) và chạy liên tục qua mọi app.
- XP dùng chung nhất quán, không cộng trùng (mission/shell tests, mutation check M2).
- Dữ liệu sau restart: mô phỏng reboot bằng fake storage cho Focus/Missions/Pet/
  Window/Weather/PC token.
- Calibration không bị đụng: không có code ghi/xóa `cyd-touch` mới; không có erase.
- Animation không blocking: dựa thời gian, giới hạn bước/vùng vẽ; không `delay()` mới.
- Wi-Fi mất/không cấu hình: UI và service vẫn chạy (weather/pc/shell tests với mạng giả).
- Weather timeout/HTTP/TLS/parse lỗi: backoff 30 s → 10 phút, watchdog 30 s, cache.
- PC Agent ngừng gửi: DISCONNECTED sau 6 s (pc_tests PC4/PC5).
- Không cấp phát heap ở trạng thái ổn định (đếm `operator new` trên host).
- Chain TLS Open-Meteo xác thực được chỉ với ISRG Root X1 (openssl, 2026-11-20).

## HARDWARE PENDING

Toàn bộ M2–M6 trên board: hiển thị/màu, cảm ứng, NVS thật, mất nguồn, Wi-Fi/TLS/NTP
thật, UDP từ PC thật, heap runtime và độ ổn định dài hạn. Xem
[HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md).

## Giới hạn và rủi ro đã biết

1. Heap runtime khi Wi-Fi + TLS chưa đo; TLS handshake cần ~40 KB liên tục. Theo dõi
   `HTTPS: … heap=` và `MEMORY … largest_free_block`.
2. Wi-Fi tăng dòng tiêu thụ; nguồn USB yếu có thể gây brownout/reset trên CYD.
3. Framework Arduino-ESP32 2.0.11 tắt `MBEDTLS_HAVE_TIME_DATE`: thiết bị kiểm chain +
   hostname nhưng không kiểm hạn hiệu lực chứng chỉ. Nếu Open-Meteo đổi sang CA
   không có trong 4 root đi kèm, cần cập nhật `src/net/RootCertificates.h`.
4. PC Monitor: dữ liệu được xác thực nhưng không mã hóa (người cùng LAN đọc được số
   liệu CPU/RAM). Counter chống replay nằm trong RAM: sau khi ESP32 reboot, một gói
   cũ bắt được trên LAN có thể được chấp nhận một lần (chỉ hiển thị số liệu cũ; giới
   hạn ±300 s khi đã có NTP).
5. Token hiển thị qua Serial và lưu dạng JSON trong `%APPDATA%\DeskOS` của người dùng.
6. Daily Missions tự sang ngày mới lúc 00:00 (UTC+7) khi đã có NTP — thay đổi hành
   vi so với M2 thủ công (NEW DAY vẫn dùng được).
7. Pet lưu số lần vuốt khi rời app; mất nguồn khi đang ở màn Pet mất các lần vuốt đó
   (không ảnh hưởng XP).
8. Test UI dùng draw spy (lệnh vẽ, bounds), không mô phỏng điểm ảnh/SPI/touch thật.
9. Python agent: số liệu mạng chỉ có khi cài `psutil`; nếu không hiển thị `NET N/A`.
