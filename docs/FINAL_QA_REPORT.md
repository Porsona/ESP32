# Final QA Report — DESKOS_1.0.0-RC1

**2026-11-20**, Asia/Saigon. Branch `release/v1.0.0` (tách từ `feature/m2-daily-missions`
tại `759405e`), tag cục bộ `v1.0.0-rc1`. ESP32-2432S028R, PlatformIO `cyd_ili9341`,
espressif32@6.4.0 (Arduino-ESP32 2.0.11), TFT_eSPI 2.5.43, XPT2046_Touchscreen v1.4.

Quy ước: **SOFTWARE PASS** = build + test host; **HARDWARE PASS** = người dùng xác
nhận trên board; **HARDWARE PENDING** = chưa có bằng chứng thiết bị; **FAIL** = lỗi mở.
Báo cáo này không chứa kết quả phần cứng mới: chưa Upload, chưa chạy trên board.

## 1. Git tại thời điểm audit

> **Bổ sung 2026-11-23 — Pixel Pet VOLT + tương tác chạm, chưa phát hành:** build **SOFTWARE PASS**,
> 83 nhóm test PASS (67 C++ + 9 PC Agent + 7 Serial logger) + kiểm dữ liệu sprite, 8 nhóm pet; RAM tĩnh 55,252 B; flash 1,068,945 B (81.6%, +30,580 B so RC1, còn 241,775 B); firmware.bin 1,074,688 B, SHA256 `EAE25DFCBFFB770B25E2264B54A25771CAD87EF64D636F4B829F7059A6A15E7D`. Xem [PET_VOLT.md](PET_VOLT.md), [log build](PET_build.log), [log test](PET_tests.log).
> Sprite/nhãn/icon và phản ứng chạm của pet được thay/thêm; XP/level, schema NVS, `AppManager`,
> phần cứng và network không đổi. Hình xem trước chỉ là kiểm tra trên máy; hình ảnh/timing/cảm ứng
> pet **HARDWARE PENDING**; VOLT chưa nạp, chưa xác nhận giao diện trên board.
> Các phần còn lại của báo cáo ghi checkpoint RC1 ngày 2026-11-20.

| Mục | Giá trị |
| --- | --- |
| HEAD trước RC1 | `759405e` Finalize M6 project documentation (sạch, đồng bộ `origin/feature/m2-daily-missions`) |
| Tag có sẵn | `m0-stable` (`654b069`), `m1-stable` (`c59cfae`) |
| File chưa commit lúc bắt đầu | Không có |
| Vùng phần cứng/M1 | `git diff m1-stable` rỗng cho `platformio.ini`, `include/`, `src/hal/`, `src/core/TouchCalibration*`, Focus model/service/storage/screen |

## 2. Audit theo module (đối chiếu source, không dựa vào báo cáo cũ)

| Hạng mục | IMPLEMENTED | SOFTWARE VERIFIED | HARDWARE |
| --- | --- | --- | --- |
| Shell, Home 6 tile, BACK/HOME, registry 7 id | Có (`main.cpp registerShell`, `AppManager`) | shell_tests S01–S03 | M0 PASS; RC1 PENDING |
| Calibration/TouchHAL/Diag | Có, không đổi từ m1-stable | — (đã nghiệm thu) | PASS (M0) |
| Focus Quest (+25 XP) | Có, không đổi từ m1-stable | focus 16 + UI 5 | PASS (M1) |
| Daily Missions (+10 XP, A/B, NEW DAY, 00:00) | Có, nối `missionService.update()` mỗi loop | mission 12 + UI 5 | PENDING |
| Pixel Pet (chỉ đọc XP) | Có (`PetService` nhận `const ProgressService&`) | pet 5 | PENDING |
| Pixel Window (AUTO từ NTP + weather) | Có (`LiveWindowContext`) | window 5 | PENDING |
| Wi-Fi / NTP / Living Weather / HTTPS | Có (`NetworkService`, `TimeServiceESP`, `HttpsFetchTask`, `WeatherService`) | weather 7 (gồm WE7 mới) | PENDING |
| PC Monitor + Agent | Có (`PcLinkUDP`, `PcMonitorService`, agent Python) | pc 6 (gồm PC6 mới) + agent 9 | PENDING |
| Logger kiểm thử phần cứng | Mới trong RC1 | logger 7 | Dùng trong bài test |

Không có tính năng nào được báo "hoàn thành" mà chưa nối vào `main.cpp`/`AppManager`:
cả 6 app được đăng ký (`SHELL_REGISTRY … dedicated_apps=6 placeholders=0`), mọi service
nền được gọi trong `loop()`.

## 3. Vấn đề tìm thấy

| ID | Loại | Mô tả | Bằng chứng | Trạng thái |
| --- | --- | --- | --- | --- |
| Q1 | Security (giới hạn) | TLS không kiểm hạn chứng chỉ: sdkconfig framework `# CONFIG_MBEDTLS_HAVE_TIME_DATE is not set` | `~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/sdkconfig:1400` | **Đã giảm thiểu**: kiểm leaf khi có NTP (K6) |
| Q2 | Security (giới hạn) | Counter chống replay chỉ trong RAM → reboot mở lại toàn bộ không gian counter | `PcMonitorService` M6 | **Đã giảm thiểu**: floor NVS (K9) |
| Q3 | Reliability (rủi ro) | HTTPS task priority 1 trên core 0 trong khi framework bật `CONFIG_ESP_TASK_WDT_PANIC=y`, timeout 5 s, theo dõi IDLE0: crypto TLS chạy liền > 5 s sẽ panic-reboot | sdkconfig:965–967 | **Đã sửa phòng ngừa**: priority 0; cần xác nhận trên board (N04, S02) |
| Q4 | Observability | `MEMORY` thiếu low-water heap/stack; `STATUS` không in Window/PC/Memory mà S04 yêu cầu | `main.cpp` M6 | **Đã sửa** |
| Q5 | Documentation | README còn mô tả "Starter v0.1"; không có CLAUDE.md; ARCHITECTURE ghi priority 1; test plan thiếu cột Serial/FAIL; thiếu QA/Known issues/Release notes | — | **Đã sửa** |
| Q6 | Documentation | `TEST_REPORT_M1.md` T01–T12 PENDING dù M1 đã được người dùng nghiệm thu | — | Ghi nhận (K20), không sửa số liệu lịch sử |

Không tìm thấy lỗi chức năng đã được chứng minh (crash, XP sai, mất dữ liệu) trong
working tree M6 qua đọc source và 71 nhóm test cũ. Các lỗi thật trước đây (chạm lặp
sau CONFIRM, kết quả thời tiết vị trí cũ, cờ discard) đã được sửa ở M2/M5.

## 4. Thay đổi trong RC1

| File | Thay đổi |
| --- | --- |
| `src/net/HttpsFetchTask.h/.cpp` | Kết nối TLS trước (chain + hostname), kiểm ngày leaf theo NTP, rồi GET trên kết nối đã xác thực; priority 0; log `cert_date`, `max_block`, `min_heap`, lỗi mbedTLS |
| `src/services/TimeModel.h` | `CivilTime`, `civilToEpoch`, `certDateStatus` (thuần, test host) |
| `src/services/NetTypes.h/.cpp` | `httpsUrlHost()` (chỉ `https://`, không cho port/userinfo) |
| `src/services/PcMonitorModel.h/.cpp` | `PcCounterRecord` 24 B (magic/version/size/token tag/CRC32) |
| `src/services/PcMonitorService.h/.cpp` | Nạp floor khi boot, checkpoint ≤ 1 lần/15 phút khi có NTP, reset khi đổi token |
| `src/main.cpp` | Version `DESKOS_1.0.0-RC1`, inject time source, `MEMORY`/`STATUS`/`PC` mở rộng |
| `src/ui/PixelRenderer.cpp` | Banner `DESKOS 1.0` |
| `tests/weather_tests.cpp`, `tests/pc_tests.cpp` | WE7, PC6 |
| `tests/run_all_tests.ps1` | Thêm test logger |
| `tools/hw_validation/*` | Logger + analyzer + test |
| `.gitignore` | `/hw_logs/`, `__pycache__/` |
| Tài liệu | README, CLAUDE.md, ARCHITECTURE, DEVELOPMENT_STATUS, HARDWARE_TEST_PLAN, USER_GUIDE, PC_AGENT_SETUP, FINAL_QA_REPORT, KNOWN_ISSUES, RELEASE_NOTES |

## 5. Build và test

| Check | Kết quả | Log |
| --- | --- | --- |
| Baseline M6 (trước sửa) | SUCCESS, RAM 52,868 B, Flash 1,038,233 B; 71/71 nhóm PASS (khớp `M6_final_tests.log`) | — |
| Clean build RC1 | SUCCESS; chỉ warning `TOUCH_CS` của TFT_eSPI (16 lần, như baseline); 0 warning từ `src/` | `RC1_clean.log`, `RC1_build.log` |
| RAM tĩnh | 52,892 / 327,680 B (16.1%), +24 B | |
| Flash | 1,038,365 / 1,310,720 B (79.2%), +132 B; còn 272,355 B trong app slot | |
| firmware.bin | 1,044,112 B, SHA256 `6BC7E7B335BDEAEE0130B8916399A05957F630ADE08B76C00481F3E18C854469` | |
| Test tự động | **80/80 nhóm PASS**: 64 C++ (focus 16, focus UI 5, mission 12, mission UI 5, pet 5, window 5, weather 7, pc 6, shell 3) + 9 PC agent + 7 logger | `RC1_tests.log` |
| Mutation check | Bỏ nạp floor → PC6 FAIL; checkpoint khi chưa NTP → PC6 FAIL; nới hạn Expired 1 ngày → WE7 FAIL (đã khôi phục) | — |
| Compiler host | GCC 5.1 mingw32, `-std=c++11 -Wall -Wextra -Werror -pedantic` | |

## 6. Rà soát độ tin cậy và bảo mật (C1–C5)

| Mục | Kết luận | Bằng chứng |
| --- | --- | --- |
| C1 Heap/stack/task | Không cấp phát ở trạng thái ổn định (đếm `operator new`); HTTPS task một lần, stack 12 KB, buffer cố định; không `delay()`; Serial parser giới hạn 32 byte/loop. Heap runtime: **HARDWARE PENDING** | S03, PC5, WE6, P05, W05 |
| C1 Watchdog/timeout | Connect 6 s, handshake 12 s, read 8 s, watchdog service 30 s; Wi-Fi backoff 10 s→2 phút; HTTPS priority 0 | WE4 |
| C2 NVS | Mọi record có magic/version/size/CRC32 + `static_assert` layout; ghi + đọc lại; Missions A/B; không có erase. Tần suất ghi: hành động người dùng, rời app, Focus ≥ 60 s, weather ≥ 15 phút, PC floor ≤ 15 phút | M06–M08, P04, W01, WE2, PC4, PC6 |
| C3 HTTPS | CA chain + hostname luôn bật, không `setInsecure`; leaf date khi có NTP; HTTP lỗi/JSON hỏng/cắt cụt/quá 2 KB → lỗi + backoff + cache | WE1, WE3, WE4, WE7 |
| C4 PC Monitor | Agent chỉ gửi (test cấm bind/listen/recv/exec/subprocess); chỉ LAN; HMAC constant-time trước khi parse; replay counter + floor NVS; skew ±300 s; timeout 6 s; rate limit 20/s; token không có trong repo | PC1–PC6, agent ×9 |
| C5 XP chung | +25/Focus, +10/quest, idempotent, ledger cùng commit với quest, không reset khi sang ngày/mất nguồn, Pet chỉ đọc | M02–M12, S02, P02 |

## 7. Hardware PENDING

Toàn bộ [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md): R01–R06, D01–D08, P01–P06,
W01–W05, N01–N12, C01–C07, S01–S05 — **PENDING**.

## 8. Kết luận

RC1: **SOFTWARE PASS**, **HARDWARE PENDING**, không có FAIL mở. Điều kiện `v1.0.0`:
xem [RELEASE_NOTES.md](RELEASE_NOTES.md#điều-kiện-để-phát-hành-v100).
