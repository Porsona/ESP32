# M2 — Daily Missions + Shared XP

**2026-11-20**, Asia/Saigon. Firmware **M2_MISSIONS_01**.
ESP32-2432S028R, 320×240, COM5 / 115200 baud.
Agent chỉ build/test trên máy; **chưa Upload, chưa kiểm thử board M2**.

## Baseline (M2.0)

- Người dùng báo M0 PASS và M1 Focus Quest PASS 10/10 hardware acceptance.
  TEST_REPORT_M1.md vẫn ghi T01–T12 PENDING vì log chi tiết chưa được chuyển
  vào repo; agent không tự sửa các dòng đó thành PASS.
- Trước khi sửa: `git status` có thay đổi M1 chưa commit (đã xem từng file). Build lại
  tạo đúng BIN `00C4C3487AFC8DF63581A4265715FA79537008E4DFBF62BD931E84AC086A6D59`
  và RAM/Flash 23,272/323,181 như TEST_REPORT_M1 → commit `c59cfae`, annotated
  tag `m1-stable`, branch `feature/m2-daily-missions`. Không ghi đè tag, không
  bỏ thay đổi nào.
- Audit API thực tế: không có EventBus và không có class StorageService.
  Storage gồm `touchcal::load/save` và `FocusStorageNVS`. XP nằm trong
  FocusRecord; `ProgressService::adopt` chỉ friend FocusSessionService.
- `git diff m1-stable` rỗng cho platformio.ini, include/, src/core/, src/hal/,
  DisplayHAL, Diagnostic, Focus model/service/storage/screen, Home,
  Placeholder, calibration/test screens, UITheme, test và fake của M1.

## Build

| Check | Kết quả | Bằng chứng |
| --- | --- | --- |
| Build sau M2.1/M2.2 (service + NVS, chưa nối main) | SUCCESS; RAM 23,288, Flash 323,737 | Console |
| Clean | SUCCESS | M2_missions_clean.log |
| Build cuối cyd_ili9341 | SUCCESS, 16.0 s; 0 warning/error từ `src/` | M2_missions_build.log |
| Static RAM | 23,824 / 327,680 byte (7.3%) | +552 byte so M1 |
| Flash | 334,861 / 1,310,720 byte (25.5%) | +11,680 byte so M1 |
| firmware.bin | 335,232 byte, SHA256 `8DA49882E27C35E422E3AACD663CA6F9FD8CA38BEBBD13E8963B6A679A1C68BB` | `.pio/build/cyd_ili9341/` |
| Upload / Serial M2 trên board | PENDING | Chưa thực hiện |
| Runtime free heap / largest block | PENDING | Static RAM không thay cho heap runtime |

Warning `TOUCH_CS pin not defined` của TFT_eSPI giống baseline (touch dùng
XPT2046 trên SPI riêng). Toolchain giữ espressif32 6.4.0, TFT_eSPI 2.5.43,
XPT2046 v1.4.

## Automated tests

Host GCC 5.1.0, C++11, `-Wall -Wextra -Werror -pedantic -static`, biên dịch
chính source production với fake clock/storage (nhảy timestamp, không chờ thời
gian thật; duration Focus vẫn 1,500,000/300,000 ms). Runner
`tests/run_mission_tests.ps1`; log `M2_missions_unit_tests.log`.

| ID | Yêu cầu | Test | Status |
| --- | --- | --- | --- |
| 1 | Load 5 nhiệm vụ mẫu | M01 | PASS |
| 2 | Complete một nhiệm vụ | M02, MU1 | PASS |
| 3 | Progress 20/40/60/80/100% | M02–M04 | PASS |
| 4 | +10 XP chính xác | M02–M04 | PASS |
| 5 | TAP lặp không cộng trùng | M05, MU1 | PASS |
| 6 | Reboot không cộng trùng | M06 | PASS |
| 7 | NVS read/write failure | M07 | PASS |
| 8 | Checkpoint hỏng (12 biến thể) + fallback A/B | M08 | PASS |
| 9 | NEW DAY reset completion, giữ XP | M09–M10, MU3 | PASS |
| 10 | Cùng questId ở dayId mới được thưởng lại | M09–M10, MU3 | PASS |
| 11 | FocusCompleted: +25 Focus và +10 mission, mỗi cái một lần | M11, MU4 | PASS |
| 12 | Reboot giữa lưu completion và reward (4 kịch bản) | M12 | PASS |
| 13 | Chuyển trang không mất trạng thái | MU2 | PASS |
| 14 | Không ảnh hưởng timer M1 | M14, MU4 + suite M1 | PASS |

| Test | Nội dung | Status |
| --- | --- | --- |
| M01 | Template, title ≤16, kind; một lần ghi đầu, reboot không ghi | PASS |
| M02–M04 | Tick 1–4 thủ công + quest Focus thật; 200→1000‰; key `mission:1:1` | PASS |
| M05 | 200 TAP + 200 update: không XP/không ghi; ID sai; quest Focus từ chối tick tay | PASS |
| M06 | 5 lần reboot: XP, tick, rewarded giữ; grantedCount=2 | PASS |
| M07 | Ghi lỗi → pending, khóa thao tác, retry 5 s đúng một lần; read I/O khóa ghi, Focus +25 vẫn chạy | PASS |
| M08 | CRC/version/size/day/ID trùng/reward không tick/title/padding/XP/ledger/reserved/kind; slot A/B chọn mới nhất, fallback, I/O | PASS |
| M09–M10 | Stale expected day; tick reset; XP/Focus record giữ nguyên; key ngày mới; ngày cũ DAY_CLOSED | PASS |
| M11 | START+RESET không hoàn thành; deadline −1 ms không; đúng deadline +25 +10; Focus thứ hai chỉ +25; reboot | PASS |
| M12 | (a) mất nguồn trước commit (b) commit nhưng báo lỗi (c) Focus đã lưu, mission chưa (d) NEW DAY báo lỗi | PASS |
| M14 | Tick/NEW DAY/QTITLE khi Focus RUNNING: remaining chính xác, không ghi record Focus | PASS |
| M15 | QTITLE chuẩn hóa/biên; đổi title không thưởng lại; guard reward key/XP/tràn số | PASS |
| M16 | Hook IDaySource cho M5: không nguồn = thủ công, ghi dấu, lùi giờ, ngày mới | PASS |
| MU1 | AppManager thật: TAP một lần; TAP trùng, kéo, giữ bị hủy | PASS |
| MU2 | PREV/NEXT, ô trống, HOME/BACK, mở lại 20 lần: không ghi, không thưởng | PASS |
| MU3 | NEW DAY: chạm lặp = CANCEL, arm 300 ms, chặn chạm sau hộp thoại, cooldown, timeout, rời màn, PRESS cũ khi ngày đổi | PASS |
| MU4 | Focus chạy qua Missions/NEW DAY; Focus kết thúc khi đang ở Missions; Break START sau đó | PASS |
| MU5 | Idle không vẽ; chỉ vẽ lại theo vùng; mọi lệnh vẽ trong 320×240 | PASS |
| M1 | Hồi quy 16 nhóm service + 5 nhóm UI của Focus với source M2 | PASS 21/21 |

Mutation check: bỏ guard `AlreadyRewarded` và `AlreadyCompleted` làm fail
M05, M06, M09–M10, M12, M15, nên test thực sự bắt được thưởng trùng.

Giới hạn: draw spy chỉ kiểm lệnh vẽ/bounds, không mô phỏng pixel GLCD, SPI hay
raw touch. Fake storage kiểm hợp đồng commit/lỗi, không chứng minh độ bền NVS
vật lý hoặc tính atomic khi cúp điện thật. Adapter NVS A/B
(`MissionStorageNVS`) chưa được test trên host; logic chọn slot được test riêng.

## Hardware acceptance — PENDING

Upload thường, không erase. Ghi XP trước/sau (STATUS) và so chênh lệch.

| ID | Cách kiểm tra → kết quả cần đạt | Status |
| --- | --- | --- |
| H01 | Upload SUCCESS; Serial `M2_MISSIONS_01`, `FOCUS_RESTORE`, `MISSION_RESTORE: result=MISSING day=1 bonus_xp=0 total_xp=<XP M1>` (lần đầu), `MISSION_STORE: SAVED slot=0 generation=1`, `SHELL_REGISTRY: READY ids=7 screen_instances=4`, `TOUCH_CAL: LOADED_FROM_NVS`, `DISPLAY_READY` | PENDING |
| H02 | MISSIONS: DAILY QUESTS, DAY 1, 0/5, 3 hàng, nút `<` mờ, màu đúng palette, chữ không cắt | PENDING |
| H03 | Tick STUDY SQL: checkbox, hiệu ứng pixel, `+10 XP! QUEST DONE`, thanh 20%, XP +10, `MISSION_REWARD: key=mission:1:1 result=GRANTED`, `MISSION_STORE: SAVED` | PENDING |
| H04 | Chạm lại/nhanh nhiều lần, kéo, giữ: XP không đổi, không có `MISSION_STORE` mới | PENDING |
| H05 | `>`/`<`, HOME/BACK, mở lại: tick và trang giữ nguyên; không có `MISSION_STORE` | PENDING |
| H06 | Chạm COMPLETE FOCUS: thông báo, không XP. Mở Focus, START→RESET: không hoàn thành. Chạy đủ 25:00: Focus +25 và quest tự tick +10 (tổng +35), `key=mission:1:5` | PENDING |
| H07 | Reboot (EN/rút nguồn) sau H03/H06: `MISSION_RESTORE: result=LOADED`, cùng day/tick/XP, không có `MISSION_REWARD` lúc boot | PENDING |
| H08 | NEW DAY: chạm lặp → CANCEL; chờ 5 s → tự hủy; CONFIRM → DAY 2, 0/5, XP giữ; Focus timer/state giữ; NEW DAY khóa 3 s | PENDING |
| H09 | Ngày 2 tick STUDY SQL: `key=mission:2:1`, +10 | PENDING |
| H10 | Focus RUNNING → Missions (tick, trang, NEW DAY) → Focus: thời gian vẫn giảm đúng | PENDING |
| H11 | `QTITLE 3 READ 20 PAGES`: SAVED, hiển thị title mới, reboot vẫn giữ, XP không đổi | PENDING |
| H12 | Hồi quy M1: Start/Pause/Resume/Reset, sáu icon Home, CAL/TEST/DIAG/NEXT/HOME | PENDING |
| H13 | ≥30 phút gồm Focus + nhiều chuyển màn Missions: không reboot; MEMORY free_heap/largest_free_block không giảm liên tục | PENDING |

Chỉ cập nhật Hxx thành PASS/FAIL từ log/quan sát người dùng gửi.

## File

Thêm: `src/services/{Crc32.h, MissionModel.h/.cpp, MissionStorage.h,
DailyMissionService.h/.cpp, FocusCompletionAdapter.h}`,
`src/storage/MissionStorageNVS.h/.cpp`, `src/apps/DailyMissionsScreen.h/.cpp`,
`tests/{mission_services_tests.cpp, mission_ui_tests.cpp, run_mission_tests.ps1}`,
`docs/{DAILY_MISSIONS.md, TEST_REPORT_M2.md, M2_missions_*.log}`.
Sửa: `src/services/ProgressService.h/.cpp` (thêm API, M1 API giữ nguyên),
`src/ui/PixelRenderer.h/.cpp` (thêm primitive, banner M2), `src/main.cpp`,
`docs/ARCHITECTURE.md`.
