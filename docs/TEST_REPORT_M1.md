# M1 — Focus Quest, Pomodoro & Shared XP

**2026-11-19**, Asia/Saigon. Firmware **M1_FOCUS_01**.
ESP32-2432S028R / ESP32-D0WD-V3, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa kiểm thử board M1**.

## Baseline

- Người dùng xác nhận M0.0/M0.1/M0.2 PASS, M0.3 PASS 8/8 trước M1.
- Ban đầu chưa có Git. Backup 41 file source/config/BIN/ELF/docs/log:
  `docs/baselines/M0_STABLE_before_M1_2026-11-19/`; SHA256 41/41 khớp.
- Commit `654b069`, annotated tag `m0-stable`, branch `feature/m1-focus-quest`.
- Baseline BIN: `E224360C21C37314F4E0946A1BFF7CDCB3F0D2F9AC7DBB489ED1B925F0483C63`.
- Diff so tag: platformio.ini, board_pins, DisplayHAL, TouchHAL, AppManager,
  calibration/store, HomeScreen và PlaceholderScreen nguyên trạng. Không thêm
  TFT/Touch/SPI instance hoặc sửa inversion/font/driver/dependencies.
- Không truy cập/xóa/ghi NVS trên thiết bị trong phiên này.

## Kết quả trên máy

| Check | Kết quả thực tế | Bằng chứng |
| --- | --- | --- |
| A clean/build service + storage | PASS / SUCCESS | M1_focus_services_clean.log, M1_focus_services_build.log |
| A resource usage | RAM 23,056; Flash 317,721 byte | Chưa nối service vào main; không đại diện bản cuối |
| Unit services/clock/storage | PASS 16/16 nhóm | M1_focus_unit_tests.log |
| Integration AppManager/Focus screen/renderer | PASS 5/5 nhóm | Cùng log; Arduino/TFT là host spy |
| Final clean | PASS / SUCCESS | M1_focus_clean.log |
| Final cyd_ili9341 build/link/BIN | PASS / SUCCESS, 14.24 s | M1_focus_build.log |
| Final static RAM | 23,272 / 327,680 byte (7.1%) | +216 byte so M0 |
| Final Flash | 323,181 / 1,310,720 byte (24.7%) | +5,864 byte so M0 |
| Upload / Serial M1 trên board | PENDING | Chưa thực hiện |
| Runtime heap/largest/stress | PENDING | Static RAM khác runtime heap |

BIN `.pio/build/cyd_ili9341/firmware.bin`, SHA256:
`00C4C3487AFC8DF63581A4265715FA79537008E4DFBF62BD931E84AC086A6D59`.

Giữ espressif32 6.4.0, Arduino ESP32 2.0.11, TFT_eSPI 2.5.43, XPT2046
d57f64c. Warning TOUCH_CS TFT_eSPI như baseline: touch dùng XPT2046 trên SPI
riêng nên không thêm TOUCH_CS cho TFT. Không lỗi compile/link. PowerShell
ghi nhãn NativeCommandError khi đưa warning stderr qua Tee-Object; exit code
PlatformIO thực tế là 0/SUCCESS.

## Automated tests

Host GCC 5.1.0, C++11, `-Wall -Wextra -Werror -pedantic -static`.
Runner `tests/run_focus_tests.ps1` biên dịch chính C++ production; clock giả
nhảy timestamp, duration giữ 1,500,000/300,000 ms. Không thêm host compiler
vào platformio.ini/lib_deps hoặc dùng timer mô phỏng bằng source khác.

| ID | Nhóm test | Status |
| --- | --- | --- |
| U01 | Defaults; Start/Pause/Resume; paused time không trừ | PASS |
| U02 | Đúng deadline 25 phút; 10,000 update không thưởng thêm | PASS |
| U03 | Break 5 phút; START thủ công; phiên Focus thứ hai | PASS |
| U04 | Reset chưa đủ không XP, giữ progress/retired IDs | PASS |
| U05 | uint32 millis rollover với Pause/Resume/Complete | PASS |
| U06 | Checkpoint trước/đúng 60 giây; PAUSED không ghi | PASS |
| U07 | Core update 25 phút giả lập khi không có screen | PASS |
| U08 | Reboot RUNNING → PAUSED; completed restore không thưởng lại | PASS |
| U09 | Missing/CRC/version/size/enum/duration/IDs/reserved/read I/O; I/O khóa ghi | PASS |
| U10 | Start/Pause/Resume/Reset write failure giữ RAM cũ | PASS |
| U11 | Checkpoint failure không spam retry; RAM timer vẫn đúng | PASS |
| U12 | Completion pending, retry snapshot, publish XP một lần | PASS |
| U13 | Ghi đã commit nhưng báo lỗi, reboot/retry không thưởng trùng | PASS |
| U14 | Power loss trước completion commit, resume rồi thưởng một lần | PASS |
| U15 | Wrong/duplicate ID và overflow guards | PASS |
| U16 | Deadline xử lý trước Pause/Reset tại ranh giới | PASS |
| I01 | AppManager thật: HOME/BACK/all IDs/suspend không mất timer | PASS |
| I02 | TAP trùng bị chặn; drag/hold cancel; Pause/Resume UI | PASS |
| I03 | RESET confirm/cancel/timeout | PASS |
| I04 | PRESS PAUSE trước deadline, TAP sau không START Break nhầm | PASS |
| I05 | Idle không render; dirty redraw không full clear; bounds 320×240 | PASS |

Draw spy kiểm tra lệnh vẽ/bounds, không mô phỏng GLCD/pixel, SPI hoặc raw touch.
Fake storage kiểm tra hợp đồng commit/failure; không chứng minh endurance/
power-cut NVS vật lý. Kiểm thử trên ESP32 vẫn PENDING.

## Hardware acceptance

Không suy diễn PASS M1 từ M0/host. Nếu đã có tiến độ M1, dùng chênh lệch XP/
session trước-sau; không erase NVS để bắt đầu lại.

| ID | Cách kiểm tra và kết quả cần đạt | Status | Evidence |
| --- | --- | --- | --- |
| T01 | Upload SUCCESS; Serial M1_FOCUS_01, DISPLAY_READY | PENDING (build PASS) | Chờ COM5 log |
| T02 | IDLE 25:00; màu/font/nút không cắt trong 320×240 | PENDING | Chờ quan sát |
| T03 | START đếm thời gian thực, kiểm tra sau 60 giây | PENDING | Chờ đo |
| T04 | PAUSE giữ ít nhất 30 giây, RESUME tiếp tục | PENDING | Chờ thử |
| T05 | RUNNING → HOME/placeholder/DIAG → Focus vẫn tiến | PENDING | Chờ thử |
| T06 | RESET cần CONFIRM; CANCEL/timeout không reset; confirm → 25:00, giữ XP | PENDING | Chờ thử |
| T07 | Đủ 25 phút +25 XP/+1 count; 20 lần ra/vào và reboot sau SAVED không thưởng lại | PENDING | Chờ đủ phiên/log |
| T08 | Break chờ START, đủ 5 phút không XP; Focus 25:00 chờ START | PENDING | Chờ đủ phiên |
| T09 | RUNNING checkpoint SAVED → reset/power cycle: PAUSED; PAUSED/COMPLETED giữ sau reboot | PENDING | Chờ log trước/sau |
| T10 | Sáu icon, BACK/HOME; tap/hold/drag không thao tác kép | PENDING | Chờ thử |
| T11 | NVS calibration load, TEST/DIAG/NEXT/CAL/HOME vẫn hoạt động | PENDING | Chờ thử |
| T12 | >=30 phút gồm Focus+Break và nhiều transition; không reboot, heap/largest không giảm liên tục bất thường | PENDING | Chờ chuỗi MEMORY |

So heap sau warm-up ở cùng trang, ghi min/max/thời điểm; không kết luận leak
từ một số đo. Chưa có heap runtime hoặc stress M1 trên phần cứng để báo cáo.

## Chính sách phục hồi

Một blob version/CRC giữ timer+progress. Checkpoint khỏe cho phép mất khoảng
<=60 giây công việc cộng độ trễ loop/ghi; ghi lỗi kéo dài có thể làm mất nhiều
hơn. Không đoán thời gian tắt nguồn. Completion lỗi giữ transaction pending,
retry 5 giây và khóa timer controls. Read I/O lỗi khóa START/RESET đến boot
đọc được. Record hỏng fallback/log lỗi, chưa tự ghi sửa đến thao tác chủ động.

## File và tái hiện

Thêm FocusModel, FocusSessionService, ProgressService, FocusStorage interface,
FocusStorageNVS, FocusClock, FocusQuestScreen và tests. Sửa main.cpp và
PixelRenderer.h/.cpp. Cập nhật ARCHITECTURE.md, thêm FOCUS_QUEST.md,
TEST_REPORT_M1.md và log build/test. `.gitignore`, M0_ACCEPTANCE.md và ghi
nhận nghiệm thu TEST_REPORT_M0.md đã nằm trong commit baseline trước M1.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_focus_tests.ps1
pio run -e cyd_ili9341 -t clean
pio run -e cyd_ili9341
```

Người dùng đóng monitor đang giữ COM5 rồi tự nạp:

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

Kiểm tra M1_FOCUS_01, DISPLAY_CONFIG: INVERT_ON, TOUCH_CAL: LOADED_FROM_NVS,
DISPLAY_READY. STATUS in Focus/calibration; MEMORY tự in 5 giây/lần. Không
erase NVS. Chỉ cập nhật Txx PASS từ kết quả người dùng. Xem thêm
[FOCUS_QUEST.md](FOCUS_QUEST.md) và [ARCHITECTURE.md](ARCHITECTURE.md).
