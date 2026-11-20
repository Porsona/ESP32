# M6 — PC Monitor + tích hợp shell

**2026-11-20**, Asia/Saigon. Firmware **M6_PCMON_01**, commit `f44efe0`
(merge `05f451c`). ESP32-2432S028R, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa thử UDP từ PC thật**.

## Phạm vi

- `PcLinkUDP`: chỉ nhận, UDP 47801, chỉ mở khi Wi-Fi lên.
- `PcMonitorService`: chỉ nhận nguồn cùng subnet; tối đa 20 gói/s;
  HMAC-SHA256 (so sánh constant-time) trước khi đọc field; counter tăng dần
  chống replay; lệch giờ ≤300 s khi cả hai đồng hồ hợp lệ; DISCONNECTED sau 6 s
  không có gói. Không có lệnh, không thực thi gì từ gói nhận được.
- Token 128-bit từ RNG phần cứng (cần RF bật), lưu `deskos-pc/token` 28 B;
  `PC TOKEN NEW` tạo token mới.
- `tools/pc_agent/deskos_agent.py`: chỉ gửi (không bind/listen), từ chối địa chỉ
  ngoài LAN, CPU/RAM/DISK/pin qua Win32 API; mạng cần `psutil`.
  Hướng dẫn: [PC_AGENT_SETUP.md](PC_AGENT_SETUP.md).
- Shell: Home có 6 app riêng (Pet, Window, Focus, Missions, PC Monitor, Weather);
  banner `DESKOS M6`.

## Build

| Check | Kết quả | Bằng chứng |
| --- | --- | --- |
| Clean | SUCCESS | M6_final_clean.log |
| Build `cyd_ili9341` | SUCCESS; 0 warning/error từ `src/` | M6_final_build.log |
| Static RAM | 52,868 / 327,680 B (16.1%) | +976 B so M5 |
| Flash | 1,038,233 / 1,310,720 B (79.2%) | +12,880 B so M5; còn ~272 KB |
| firmware.bin | 1,043,984 B, SHA256 `41211AAB26695D850F641052B198B73401821A550E858B0C2A58FCF55B0506C1` | `.pio/build/cyd_ili9341/` |
| Build lại 2026-11-20 (phiên kiểm tra) | SUCCESS, cùng RAM/Flash, **cùng SHA256** → build tái lập được | — |
| Upload / Serial trên board | PENDING | Chưa thực hiện |

## Automated tests

`tests/run_all_tests.ps1`, log `M6_final_tests.log`. Chạy lại 2026-11-20: cùng kết quả.
Tổng **71 nhóm PASS**: 62 C++ + 9 Python.

| Test | Nội dung | Status |
| --- | --- | --- |
| PC1 | Vector SHA-256 FIPS, HMAC RFC 4231 #1/#2/#6 | PASS |
| PC2 | Parse DPM1, sửa gói / sai key, field lạ/trùng/ngoài khoảng/quá dài | PASS |
| PC3 | Gói do agent Python thật tạo ra được firmware xác thực | PASS |
| PC4 | Replay, nguồn ngoài LAN, lệch giờ, timeout, flood, vòng đời token | PASS |
| PC5 | Trạng thái dashboard/timeout, bounds, HOME/BACK, 0 cấp phát | PASS |
| S01 | Cả 6 app mở từ tile Home; HOME và BACK quay về | PASS |
| S02 | Focus timer chạy qua cả vòng app; XP dùng chung Focus/Missions/Pet, không cộng trùng | PASS |
| S03 | 120 lần chuyển app có animation: 0 cấp phát heap | PASS |
| Python ×9 | HMAC, clamp, LAN-only, counter, host sanitize, token, send-only | PASS |
| Hồi quy | M1 21, M2 17, M3 5, M4 5, M5 6 | PASS |

## Hardware acceptance — PENDING

Theo [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) mục 6 (C01–C06) và mục 7 (S01–S04):
tạo/in token, agent kết nối và số liệu khớp Task Manager, DISCONNECTED ≤6 s khi
dừng agent, token sai bị từ chối, `PC TOKEN NEW`, tắt Wi-Fi không treo;
chạy ổn định ≥60 phút, không reset/brownout, heap không giảm dần, `STATUS` cuối
giữ đúng calibration/XP/quest/pet/window/weather/PC. Tất cả **PENDING**.

## Rủi ro đã biết

1. Dữ liệu PC được xác thực nhưng không mã hóa: người cùng LAN đọc được CPU/RAM.
2. Counter chống replay nằm trong RAM: sau khi ESP32 reboot, một gói cũ bắt được
   trên LAN có thể được nhận một lần (chỉ hiện số liệu cũ; giới hạn ±300 s khi có NTP).
3. Token được in qua Serial và lưu dạng JSON trong `%APPDATA%\DeskOS\agent.json`.
4. Không có `psutil` → mạng hiện `NET N/A`.

## File

Thêm: `src/net/PcLinkUDP.h/.cpp`, `src/services/{Sha256,PcMonitorModel,PcMonitorService}.h/.cpp`,
`src/services/NetTypes.cpp`, `src/apps/PcMonitorScreen.h/.cpp`, `tests/{pc_tests,shell_tests}.cpp`,
`tools/pc_agent/{deskos_agent,test_deskos_agent}.py`, `docs/{DEVELOPMENT_STATUS,HARDWARE_TEST_PLAN}.md`.
Sửa: `src/main.cpp`, `src/services/WeatherService.cpp` (chuyển `netStateName`/`formatIPv4`
sang NetTypes.cpp), `src/ui/PixelRenderer.cpp` (banner), `tests/run_all_tests.ps1`,
`docs/ARCHITECTURE.md`.
