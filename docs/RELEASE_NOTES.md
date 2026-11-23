# Release Notes

## Chưa phát hành — Pixel Pet VOLT + tương tác chạm — 2026-11-23

- Pixel Pet là **VOLT**, robot mascot nguyên bản của DeskOS (48×48 phóng 2× trong vùng 96×96 cũ):
  mũ bạc hà, visor tối với mắt LED, ăng-ten và tai bu-lông vàng, thân tím có lõi phát sáng, găng
  hồng, ủng vàng — đúng bảng màu Retro Pixel.
- **Tương tác chạm:** chạm đầu = xoa đầu (vẫy + tim), chạm bụng = cù (cười, "HA!"), chạm khoảng
  trống hai bên = bắn móc cáp về phía đó, vuốt = xoa đầu, giữ > 0,7 s = treo ngược đung đưa 4 s;
  chạm khi ngủ sẽ đánh thức. Phản ứng chỉ là trạng thái hiển thị: không ghi NVS, không cộng XP;
  `AppManager` không đổi.
- IDLE thở + chớp mắt; SLEEP ngồi xếp bằng; CELEBRATE giơ tay + nhún. STAR EYES, HEART EYES,
  CROWN, AURA giữ mức mở khóa cũ. Icon Home là đầu robot.
- Hình sinh bằng `tools/pet_art/generate_pet.py` → `src/ui/PetSpriteData.h` (11 màu); firmware
  ghép frame + mắt LED + vương miện vào buffer tĩnh 2.352 B; treo ngược = lật dọc frame Cheer tại
  chỗ. Không heap, không xóa toàn màn khi animation.
- Giữ nguyên record `deskos-pet/state` 24 B version 1, XP/level, phần cứng, cảm ứng, Wi-Fi,
  token PC và vị trí thời tiết.
- **SOFTWARE PASS / HARDWARE PENDING**; số liệu build/test trong [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md).
  VOLT chưa nạp lên board; chưa tag.
- [Báo cáo, ảnh xem trước và cách nạp](PET_VOLT.md).

## v1.0.0-rc1 — 2026-11-20 (Release Candidate, HARDWARE PENDING)

Firmware `DESKOS_1.0.0-RC1`, banner `DESKOS 1.0`. Bản ứng viên phát hành đầu tiên
gồm đủ 6 ứng dụng. **SOFTWARE PASS**; chưa nghiệm thu trên thiết bị (M2–M6 + RC1
HARDWARE PENDING). `v1.0.0` chỉ được tag sau khi người dùng xác nhận
[HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) đạt.

### Tính năng (M0–M6)

- Shell Retro Pixel 320×240, Home 6 tile, BACK/HOME, calibration cảm ứng affine lưu NVS,
  màn TEST/DIAG (M0, đã nghiệm thu).
- Focus Quest: Pomodoro 25/5, +25 XP, chạy nền qua mọi app, checkpoint NVS (M1, đã nghiệm thu).
- Daily Missions: 5 quest/ngày, +10 XP idempotent, NEW DAY, đổi tên quest, NVS A/B (M2).
- Pixel Pet: level theo XP chung, mood, style mở theo level (M3).
- Pixel Window: DAY/NIGHT/RAIN/AUTO, 3 tốc độ, không vẽ đè khung (M4).
- Wi-Fi (cấu hình qua Serial), NTP UTC+7, Living Weather qua HTTPS Open-Meteo, cache offline (M5).
- PC Monitor + Windows Python Agent: UDP LAN, HMAC-SHA256, chỉ gửi (M6).

### Thay đổi trong RC1 (so với M6_PCMON_01)

- **TLS:** kiểm hạn hiệu lực (notBefore/notAfter) chứng chỉ leaf của server sau
  handshake khi đã có giờ NTP; từ chối kết nối nếu hết hạn/chưa hiệu lực. Chain CA và
  hostname vẫn kiểm như trước. Log `HTTPS:` thêm `cert_date=`, `max_block=`, `min_heap=`;
  lỗi kết nối in mã lỗi mbedTLS.
- **Độ ổn định:** HTTPS task chạy priority 0 (ngang IDLE) trên core 0 để không bỏ đói
  IDLE0 — framework bật task-watchdog panic 5 s.
- **PC Monitor:** chống replay sau reboot bằng counter floor lưu NVS (`deskos-pc/ctr`,
  24 B, gắn với token, ≤ 1 lần/15 phút, chỉ khi có NTP). Giao thức DPM1 và agent không đổi.
- **Quan sát:** `MEMORY` thêm `min_free_heap`, `loop_stack_free`; `STATUS` in thêm
  `WINDOW`, `PC` và `MEMORY`; `PC` in `token=` và `replay_floor=`.
- **Công cụ:** `tools/hw_validation/deskos_serial_log.py` — ghi log Serial có che
  token/mật khẩu/SSID, tự gửi `STATUS` định kỳ, tóm tắt reset/brownout/xu hướng heap/HTTPS.
- **Tài liệu:** README mới, `CLAUDE.md`, FINAL_QA_REPORT, KNOWN_ISSUES, RELEASE_NOTES,
  HARDWARE_TEST_PLAN theo định dạng ID/bước/kỳ vọng/Serial/kết quả/file cần kiểm.

### Nâng cấp từ M1/M6

Nạp bình thường, **không erase**. Toàn bộ record NVS cũ giữ nguyên định dạng
(calibration, Focus, Missions, Pet, Window, Weather, Wi-Fi, token PC). Chỉ thêm key
mới `deskos-pc/ctr`. Quay lại `m1-stable` vẫn an toàn (namespace mới bị bỏ qua).

### Kiểm chứng

| Hạng mục | Kết quả |
| --- | --- |
| `pio run -e cyd_ili9341` (clean) | SUCCESS, 0 warning từ `src/` |
| RAM tĩnh | 52,892 / 327,680 B (16.1%) |
| Flash | 1,038,365 / 1,310,720 B (79.2%), còn 272,355 B |
| firmware.bin | 1,044,112 B, SHA256 `6BC7E7B335BDEAEE0130B8916399A05957F630ADE08B76C00481F3E18C854469` |
| Test tự động | 80/80 nhóm PASS (64 C++ + 9 PC Agent + 7 logger) |
| Phần cứng | PENDING |

Giới hạn còn lại: [KNOWN_ISSUES.md](KNOWN_ISSUES.md). Báo cáo QA: [FINAL_QA_REPORT.md](FINAL_QA_REPORT.md).

### Điều kiện để phát hành v1.0.0

1. Người dùng chạy toàn bộ [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) (R, D, P, W, N, C, S)
   trên ESP32-2432S028R và gửi log đã che secrets.
2. Không có FAIL mở; mọi ngoại lệ được người dùng chấp nhận và ghi vào KNOWN_ISSUES.
3. Stress test ≥ 60 phút: không reboot, heap không giảm liên tục, `STATUS` cuối đúng.
4. Cập nhật DEVELOPMENT_STATUS/FINAL_QA_REPORT với kết quả phần cứng, đổi
   `FIRMWARE_VERSION` thành `DESKOS_1.0.0`, build lại, test lại, rồi mới tag `v1.0.0`.
