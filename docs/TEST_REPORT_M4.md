# M4 — Pixel Window

**2026-11-20**, Asia/Saigon. Firmware **M4_WINDOW_01**, commit `200f4ab`.
ESP32-2432S028R, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa kiểm thử board M4**.

## Phạm vi

- `WindowAnimator`: khung gỗ (8,36,240×160), 4 ô kính, thanh chia dọc/ngang
  không bao giờ bị vẽ đè. Mỗi ô kính được clip và phục hồi dải màu trời.
- Scene DAY (3 mây, mặt trời có quầng), NIGHT (28 sao lấp lánh, trăng khuyết),
  RAIN (36 hạt mưa, chớp), AUTO.
- Tốc độ SLOW/NORMAL/FAST; số bước mỗi `update()` bị giới hạn nên loop bị trễ
  cũng không làm animation "bắt kịp" một mạch.
- AUTO chỉ dùng `IWindowContext` khi đồng hồ hợp lệ. Ở M4 chưa có đồng hồ →
  hiện "NO CLOCK" và cảnh DAY (không đoán giờ). M5 nối NTP + thời tiết vào hook này.
- Record `deskos-win/state` 16 B (version + CRC32), chỉ ghi khi rời app nếu đổi.

## Build

| Check | Kết quả | Bằng chứng |
| --- | --- | --- |
| Build `cyd_ili9341` | SUCCESS, 16.9 s; 0 warning/error từ `src/` | M4_window_build.log |
| Static RAM | 24,464 / 327,680 B (7.5%) | +512 B so M3 |
| Flash | 347,641 / 1,310,720 B (26.5%) | +5,616 B so M3 |
| firmware.bin SHA256 | Không ghi lại ở checkpoint này | — |
| Upload / Serial trên board | PENDING | Chưa thực hiện |

## Automated tests

Log `M4_window_tests.log` (gồm hồi quy M1–M3: 48 nhóm, 0 failure).

| Test | Nội dung | Status |
| --- | --- | --- |
| W01 | Validate record, fallback khi hỏng / lỗi I/O | PASS |
| W02 | Mở app, HOME/BACK, chạm + SCENE/SPEED, chỉ lưu khi rời app | PASS |
| W03 | AUTO chỉ dùng context đồng hồ/thời tiết hợp lệ | PASS |
| W04 | Thời gian bước SLOW/NORMAL/FAST, giới hạn khi loop bị trễ | PASS |
| W05 | DAY/NIGHT/RAIN mỗi cảnh 20,000 bước: luôn trong ô kính, không đè khung/thanh chia, 0 cấp phát | PASS |
| Hồi quy | M1 21, M2 17, M3 5 | PASS |

Giới hạn: kiểm hình học lệnh vẽ, không kiểm màu thật hay tốc độ SPI. Tốc độ
khung hình thực tế (đặc biệt FAST + RAIN) cần quan sát trên board.

## Hardware acceptance — PENDING

Theo [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) mục 4.

| ID | Tóm tắt | Status |
| --- | --- | --- |
| W01 | DAY: mây trôi, mặt trời, đồi; khung và thanh chia không bị vẽ đè | PENDING |
| W02 | Chạm/SCENE: NIGHT, RAIN, AUTO | PENDING |
| W03 | SPEED: SLOW/NORMAL/FAST khác biệt rõ | PENDING |
| W04 | Chưa có Wi-Fi: AUTO hiện "NO CLOCK" + DAY | PENDING |
| W05 | Rời app rồi reboot: scene/speed giữ (`WINDOW_RESTORE`) | PENDING |

## File

Thêm: `src/services/WindowSettings.h/.cpp`, `src/ui/WindowAnimator.h/.cpp`,
`src/apps/PixelWindowScreen.h/.cpp`, `tests/window_tests.cpp`.
Sửa: `src/main.cpp`, `tests/fakes/TFT_eSPI.h`, `tests/run_all_tests.ps1`.
