# M5 — Wi-Fi, NTP, Living Weather

**2026-11-20**, Asia/Saigon. Firmware **M5_WEATHER_01**, commit `80d0ebd`.
ESP32-2432S028R, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa thử Wi-Fi/TLS/NTP thật trên board**.

## Phạm vi

- `NetworkService`: Wi-Fi STA không blocking. SSID/mật khẩu chỉ nhập qua Serial,
  lưu NVS `deskos-net`, mật khẩu không bao giờ được in ra. Wi-Fi tắt hẳn khi chưa
  cấu hình. Reconnect backoff 10 s → 2 phút. `WiFi.persistent(false)`.
- `TimeServiceESP`: SNTP (pool.ntp.org, time.google.com, time.cloudflare.com),
  TZ UTC+7. Chỉ coi là hợp lệ sau khi sync trong lần boot hiện tại.
- `HttpsFetchTask`: một task FreeRTOS cố định trên core 0, stack 12 KB, buffer
  URL 256 B + body 2 KB. Timeout connect 6 s / handshake 12 s / read 8 s.
  `WiFiClientSecure` với 4 root CA (ISRG X1/X2, GTS R1, DigiCert G2); kiểm
  chain + hostname luôn bật, không dùng `setInsecure`.
- `WeatherService`: Open-Meteo "current" mỗi 15 phút; lỗi → backoff 30 s → 10 phút;
  watchdog 30 s; REFRESH thủ công cách nhau ≥30 s; cache `deskos-wthr/cache`;
  dữ liệu >3 giờ coi là cũ; `WEATHER LOC` đổi vị trí.
- Hook M2/M4: Daily Missions tự mở ngày mới lúc 00:00 giờ Việt Nam khi đã có NTP;
  Pixel Window AUTO dùng giờ thật + `raining()`.

## Build

| Check | Kết quả | Bằng chứng |
| --- | --- | --- |
| Build `cyd_ili9341` | SUCCESS, 22.1 s; 0 warning/error từ `src/` | M5_weather_build.log |
| Static RAM | 51,892 / 327,680 B (15.8%) | +27,428 B so M4 (Wi-Fi + lwIP + mbedTLS) |
| Flash | 1,025,353 / 1,310,720 B (78.2%) | +677,712 B so M4 |
| Partition | Không đổi (app 1,310,720 B) | Còn ~285 KB |
| firmware.bin SHA256 | Không ghi lại ở checkpoint này | — |
| Heap runtime khi TLS | PENDING | Chỉ đo được trên board |

## Automated tests

Log `M5_weather_tests.log` (gồm hồi quy M1–M4: 54 nhóm, 0 failure).
Mạng, đồng hồ và fetcher đều là fake; parser chạy trên body Open-Meteo thật đã lưu.

| Test | Nội dung | Status |
| --- | --- | --- |
| WE1 | Parser Open-Meteo (mẫu thật), body hỏng/cắt cụt, bảng mã WMO | PASS |
| WE2 | Tọa độ, URL, ranh giới ngày giờ ICT, record cache/vị trí | PASS |
| WE3 | Offline, NTP grace, lịch 15 phút, restore cache, dữ liệu cũ | PASS |
| WE4 | Lỗi HTTP/TLS/parse, watchdog timeout, trần backoff, reconnect | PASS |
| WE5 | Giới hạn REFRESH thủ công, đổi vị trí, `raining()` | PASS |
| WE6 | Màn hình: REFRESH khi offline, các trạng thái, bounds, 0 cấp phát | PASS |
| Hồi quy | M1 21, M2 17, M3 5, M4 5 | PASS |

Lỗi thật test bắt được và đã sửa: kết quả của vị trí cũ bị gán cho vị trí mới khi
đổi vị trí lúc đang fetch; cờ discard không được xóa khi watchdog timeout.

Chain TLS của api.open-meteo.com xác thực được chỉ với ISRG Root X1 (kiểm bằng
openssl trên PC, 2026-11-20).

## Hardware acceptance — PENDING

Theo [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) mục 5 (N01–N11): kết nối Wi-Fi,
giờ NTP, HTTPS 200 + dữ liệu đúng thực tế, UI không treo khi tải, REFRESH rate
limit, mất/khôi phục Wi-Fi, cache khi reboot offline, Window AUTO, qua 00:00,
đổi vị trí, `WIFI OFF/ON`. Tất cả **PENDING**.

## Rủi ro đã biết

1. Heap runtime khi Wi-Fi + TLS chưa đo; handshake cần ~40 KB liên tục. Theo dõi
   `HTTPS: … heap=` và `MEMORY … largest_free_block`.
2. Wi-Fi tăng dòng tiêu thụ; nguồn USB yếu có thể gây brownout.
3. Arduino-ESP32 2.0.11 tắt `MBEDTLS_HAVE_TIME_DATE`: firmware kiểm chain +
   hostname nhưng không kiểm hạn hiệu lực chứng chỉ. Nếu Open-Meteo đổi sang CA
   không có trong 4 root, cần cập nhật `src/net/RootCertificates.h`.
4. Tự sang ngày mới lúc 00:00 là thay đổi hành vi so với M2 (thủ công).

## File

Thêm: `src/net/{NetworkService,HttpsFetchTask,TimeServiceESP}.h/.cpp`,
`src/net/RootCertificates.h`, `src/services/{WeatherModel,WeatherService}.h/.cpp`,
`src/services/{NetTypes,TimeModel,LiveWindowContext}.h`, `src/ui/WeatherSprites.h/.cpp`,
`src/apps/WeatherScreen.h/.cpp`, `tests/weather_tests.cpp`.
Sửa: `src/main.cpp`, `.gitignore` (mẫu secrets), `tests/run_all_tests.ps1`.
