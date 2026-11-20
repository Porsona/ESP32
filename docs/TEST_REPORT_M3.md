# M3 — Pixel Pet

**2026-11-20**, Asia/Saigon. Firmware **M3_PET_01**, commit `592f658`.
ESP32-2432S028R, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa kiểm thử board M3**.

## Phạm vi

- `PetModel`: level L đạt khi XP ≥ 50·L·(L−1) (LV2 = 100, LV3 = 300, LV4 = 600 …),
  tối đa LV99. Style mở khóa ở LV1/3/5/7/10: CLASSIC, STAR EYES, HEART EYES,
  CROWN, AURA.
- `PetService`: **chỉ đọc** XP từ `ProgressService::totalXP()` (Focus + Missions),
  không bao giờ cộng/trừ XP. Mood IDLE / HAPPY (2.5 s) / SLEEP (sau 45 s không
  chạm) / CELEBRATE (3.5 s khi lên level).
- Record `deskos-pet/state` 24 B (version + CRC32) qua `BlobStoreNVS`; chỉ ghi khi
  rời app nếu có thay đổi, hoặc khi đổi style.
- `PixelRenderer::drawCharSprite`: sprite 16×16 vẽ cả ô nền → không nhấp nháy,
  không framebuffer.

## Build

| Check | Kết quả | Bằng chứng |
| --- | --- | --- |
| Build `cyd_ili9341` | SUCCESS, 15.8 s; 0 warning/error từ `src/` | M3_pet_build.log |
| Static RAM | 23,952 / 327,680 B (7.3%) | +128 B so M2 |
| Flash | 342,025 / 1,310,720 B (26.1%) | +7,164 B so M2 |
| firmware.bin SHA256 | Không ghi lại ở checkpoint này | — |
| Upload / Serial trên board | PENDING | Chưa thực hiện |

Chỉ có warning `TOUCH_CS pin not defined` của TFT_eSPI như baseline M0.

## Automated tests

Host GCC 5.1, C++11, `-Wall -Wextra -Werror -pedantic -static`; log
`M3_pet_tests.log` (gồm cả hồi quy M1/M2: 43 nhóm, 0 failure).

| Test | Nội dung | Status |
| --- | --- | --- |
| P01 | Ngưỡng level, thanh tiến độ, bảng mở khóa, sprite | PASS |
| P02 | Mood, TAP, sleep/wake, lên level trực tiếp; XP không bao giờ đổi | PASS |
| P03 | Style khóa/mở, chọn frame | PASS |
| P04 | Chỉ lưu khi rời app, restore, record hỏng, lỗi ghi | PASS |
| P05 | Mọi lệnh vẽ trong 320×240, không vẽ lại toàn màn, 0 cấp phát heap ổn định | PASS |
| Hồi quy | M1 16 + 5, M2 12 + 5 | PASS |

Giới hạn: draw spy không mô phỏng pixel/SPI; độ mượt animation và cảm giác chạm
chỉ đánh giá được trên board.

## Hardware acceptance — PENDING

Theo [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) mục 3, chạy sau mục 1 (hồi quy M0/M1).

| ID | Tóm tắt | Status |
| --- | --- | --- |
| P01 | BIT 96×96, LV đúng theo XP, XP bằng `STATUS` | PENDING |
| P02 | Chạm BIT: HAPPY + tim, PETS tăng, XP không đổi | PENDING |
| P03 | 45 s yên → SLEEP; chạm/nút để tỉnh | PENDING |
| P04 | Style khóa dưới LV3; đổi style khi đủ level, giữ sau reboot | PENDING |
| P05 | Kiếm XP rồi mở PET: "+N XP!" / "LEVEL UP!" | PENDING |
| P06 | Không nhấp nháy toàn màn; HOME/BACK | PENDING |

Giới hạn đã biết: mất nguồn khi đang ở màn Pet làm mất số lần vuốt của phiên đó
(không ảnh hưởng XP).

## File

Thêm: `src/services/{PetModel,PetService}.h/.cpp`, `src/services/BlobStore.h`,
`src/storage/BlobStoreNVS.h/.cpp`, `src/ui/PetSprites.h/.cpp`,
`src/apps/PixelPetScreen.h/.cpp`, `tests/pet_tests.cpp`, `tests/fakes/TestSupport.h`,
`tests/run_all_tests.ps1`. Sửa: `src/main.cpp`, `src/ui/PixelRenderer.h/.cpp`,
`src/services/ProgressService.h` (một dòng friend).
