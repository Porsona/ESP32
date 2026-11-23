# CYD DeskOS — Development Status

Cập nhật 2026-11-20. Bản hiện hành **DESKOS_1.0.0-RC1** (branch `release/v1.0.0`,
tag cục bộ `v1.0.0-rc1`). Không push, không upload, không erase NVS.

Quy ước: **SOFTWARE PASS** = build + test host; **HARDWARE PASS** = người dùng xác
nhận trên board; **HARDWARE PENDING** = chưa có bằng chứng thiết bị; **FAIL** = lỗi
chưa giải quyết (hiện không có).

## Tổng quan

### Pixel Pet VOLT + tương tác chạm — 2026-11-23 (chưa phát hành)

Pet là VOLT, robot mascot nguyên bản của DeskOS (48×48 phóng 2×, vẫn 96×96), với tương tác
chạm: xoa đầu, cù bụng, bắn móc cáp hai bên, vuốt, giữ để treo ngược; phản ứng không ghi NVS,
không cộng XP. **SOFTWARE PASS**: 83 nhóm test PASS (67 C++ + 9 PC Agent + 7 Serial logger) + kiểm dữ liệu sprite, 8 nhóm pet; RAM tĩnh 55,252 B; flash 1,068,945 B (81.6%, +30,580 B so RC1, còn 241,775 B); firmware.bin 1,074,688 B, SHA256 `EAE25DFCBFFB770B25E2264B54A25771CAD87EF64D636F4B829F7059A6A15E7D`. VOLT chưa nạp; board đang chạy bản prototype trước
(đã nạp COM6 theo yêu cầu chủ board, boot/NVS/network OK qua Serial). Hình ảnh/cảm ứng
**HARDWARE PENDING**. Chi tiết và log: [PET_VOLT.md](PET_VOLT.md).

Bảng dưới là checkpoint RC1 gốc; không thay đổi các kết quả nghiệm thu lịch sử.

| Milestone | Firmware | Commit | Software | Hardware |
| --- | --- | --- | --- | --- |
| M0 Shell/Touch/Diag | M0_SHELL_01 | `654b069` (`m0-stable`) | PASS | **PASS** (người dùng) |
| M1 Focus Quest | M1_FOCUS_01 | `c59cfae` (`m1-stable`) | PASS 21/21 | **PASS** (người dùng, 10/10) |
| M2 Daily Missions | M2_MISSIONS_01 | `70099fc` | PASS 17/17 | PENDING |
| M3 Pixel Pet | M3_PET_01 | `592f658` | PASS 5/5 | PENDING |
| M4 Pixel Window | M4_WINDOW_01 | `200f4ab` | PASS 5/5 | PENDING |
| M5 Living Weather | M5_WEATHER_01 | `80d0ebd` | PASS 7/7 (gồm WE7 RC1) | PENDING |
| M6 PC Monitor | M6_PCMON_01 | `f44efe0`, docs `759405e` | PASS 6/6 C++ + 9/9 Python | PENDING |
| Tích hợp shell | — | — | PASS 3/3 | PENDING |
| **RC1 hardening** | DESKOS_1.0.0-RC1 | tag `v1.0.0-rc1` | PASS (80/80 nhóm) | PENDING |

Báo cáo: [FINAL_QA_REPORT.md](FINAL_QA_REPORT.md) (hiện hành),
[KNOWN_ISSUES.md](KNOWN_ISSUES.md), [RELEASE_NOTES.md](RELEASE_NOTES.md); lịch sử
[M0](TEST_REPORT_M0.md)…[M6](TEST_REPORT_M6.md). Hướng dẫn: [USER_GUIDE.md](USER_GUIDE.md),
[PC_AGENT_SETUP.md](PC_AGENT_SETUP.md). Nghiệm thu: [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md).

## Build (PlatformIO `cyd_ili9341`, espressif32 6.4.0)

| Mốc | Static RAM | Flash | Log |
| --- | --- | --- | --- |
| M1 (baseline) | 23,272 B (7.1%) | 323,181 B (24.7%) | M1_focus_build.log |
| M2 | 23,824 B (7.3%) | 334,861 B (25.5%) | M2_missions_build.log |
| M3 | 23,952 B (7.3%) | 342,025 B (26.1%) | M3_pet_build.log |
| M4 | 24,464 B (7.5%) | 347,641 B (26.5%) | M4_window_build.log |
| M5 | 51,892 B (15.8%) | 1,025,353 B (78.2%) | M5_weather_build.log |
| M6 | 52,868 B (16.1%) | 1,038,233 B (79.2%) | M6_final_build.log |
| **RC1 (clean build)** | **52,892 B (16.1%)** | **1,038,365 B (79.2%)** | RC1_clean.log, RC1_build.log |

RC1 firmware.bin 1,044,112 B, SHA256
`6BC7E7B335BDEAEE0130B8916399A05957F630ADE08B76C00481F3E18C854469`. App slot
1,310,720 B → còn 272,355 B; không cần đổi partition. Chỉ còn warning `TOUCH_CS pin
not defined` của TFT_eSPI; 0 warning từ `src/`.

## Automated tests (host GCC 5.1, C++11, `-Wall -Wextra -Werror -pedantic`)

`powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_all_tests.ps1`
(log: `docs/RC1_tests.log`). Tổng **80 nhóm PASS** (64 C++ + 16 Python).

| Suite | Nhóm | Nội dung chính |
| --- | --- | --- |
| focus_services / focus_ui | 16 + 5 | Timer 25/5, +25 XP, checkpoint, restore, UI |
| mission_services / mission_ui | 12 + 5 | Quest, +10 XP idempotent, NVS A/B, reboot, FocusCompleted, NEW DAY |
| pet_tests | 5 | Level/unlock, mood, TAP, lưu khi rời app, record hỏng, 0 cấp phát |
| window_tests | 5 | Scene/speed, AUTO không đoán giờ, không vẽ đè khung, 0 cấp phát |
| weather_tests | 7 | Parser, timeout/backoff/watchdog, cache/offline, vị trí, **WE7 hạn chứng chỉ + host URL** |
| pc_tests | 6 | SHA-256/HMAC, DPM1, replay/subnet/flood, timeout, agent cross-check, **PC6 replay floor qua reboot** |
| shell_tests | 3 | 6 app từ Home, HOME/BACK, timer qua mọi app, XP chung, 0 cấp phát |
| tools/pc_agent | 9 | HMAC, clamp, LAN-only, counter, send-only |
| tools/hw_validation | 7 | Che token/mật khẩu/SSID, phát hiện reset/brownout/leak/phân mảnh, chỉ gửi STATUS |

## RC1 so với M6

Xem [FINAL_QA_REPORT.md §3–4](FINAL_QA_REPORT.md): kiểm ngày chứng chỉ leaf khi có
NTP, HTTPS task priority 0 (task-watchdog panic của framework), replay floor NVS cho
PC Monitor, log heap/stack chi tiết, `STATUS` đầy đủ cho S04, logger phần cứng có che
secrets, tài liệu phát hành.

## HARDWARE PENDING

M2–M6 + RC1 trên board: hiển thị/màu, cảm ứng, NVS thật, mất nguồn, Wi-Fi/TLS/NTP
thật, UDP từ PC thật, heap runtime, stress ≥ 60 phút. Bước tiếp theo: người dùng chạy
[HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) và gửi log (đã che secrets).

## Giới hạn và rủi ro đã biết

Danh sách đầy đủ: [KNOWN_ISSUES.md](KNOWN_ISSUES.md) (K1–K22).
