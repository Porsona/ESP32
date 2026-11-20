# Hardware Test Plan — DESKOS_1.0.0-RC1

Nguồn nghiệm thu chính cho `v1.0.0`. Mọi mục đang **PENDING** cho đến khi người dùng
gửi kết quả + log Serial. Agent (Claude) không Upload, không erase, không tự ghi PASS.

Cách ghi kết quả: thay `PENDING` bằng `PASS` / `FAIL` (kèm dòng Serial và mô tả/ảnh).
Khi FAIL: gửi log; cột "Nếu FAIL" chỉ file cần xem và test hồi quy liên quan.

## 0. Chuẩn bị (bắt buộc, theo thứ tự)

1. Đóng mọi Serial Monitor đang giữ COM5.
2. **Ghi trạng thái cũ** (firmware hiện tại trên board): mở monitor, gõ `STATUS`,
   lưu log (calibration, XP, sessions). Không erase flash/NVS.
3. Nguồn USB tốt (cáp ngắn, cổng đủ dòng / hub có nguồn) — Wi-Fi tăng dòng.
4. Build + Upload bản RC1 từ thư mục project:

   ```powershell
   pio run -e cyd_ili9341
   pio run -e cyd_ili9341 -t upload --upload-port COM5
   ```

   SHA256 `firmware.bin` mong đợi (clean build 2026-11-20):
   `6BC7E7B335BDEAEE0130B8916399A05957F630ADE08B76C00481F3E18C854469` (1,044,112 B).
   Kiểm: `Get-FileHash .pio\build\cyd_ili9341\firmware.bin`.
5. Thu log có che secrets (khuyên dùng thay cho `pio device monitor`):

   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\hw_validation\deskos_serial_log.py capture --port COM5 --reset
   ```

   `--reset` khởi động lại board một lần để bắt log boot. Log lưu ở `hw_logs\`
   (git-ignored), token PC / SSID / `WIFI PASS` đã được thay bằng `<REDACTED>`.
   Lệnh gõ tay (WIFI, PC TOKEN…) vẫn dùng `pio device monitor` — logger chỉ gửi `STATUS`.
   Phân tích log có sẵn: `… deskos_serial_log.py analyze hw_logs\<file>.log`.

## 1. Boot và hồi quy M0/M1 (làm trước tiên)

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| R01 | Reset board | Boot vào Home, không phải calibrate lại | `FIRMWARE_VERSION: DESKOS_1.0.0-RC1`, `DISPLAY_CONFIG: INVERT_ON`, `FOCUS_RESTORE: result=1`, `MISSION_RESTORE: result=…`, `PET: …`, `WINDOW_RESTORE: …`, `WEATHER: …`, `NET: state=…`, `SHELL_REGISTRY: READY ids=7 dedicated_apps=6 placeholders=0`, `TOUCH_CAL: LOADED_FROM_NVS`, `DISPLAY_READY` | PENDING | `src/main.cpp` setup(); không có `rst:`/`Brownout` |
| R02 | `STATUS` | XP/sessions bằng giá trị ở bước 0.2 | `FOCUS: … xp=<cũ> sessions=<cũ>`, `TOUCH_CAL: AFFINE …` | PENDING | `src/storage/FocusStorageNVS.cpp` (không đổi từ m1-stable) |
| R03 | Nhìn Home | 6 tile, màu Retro (nền tím đậm, viền mint, chữ vàng), banner `DESKOS 1.0`, chữ không bị cắt | — | PENDING | `src/apps/HomeScreen.cpp`, `src/ui/PixelRenderer.cpp`; test `shell_tests` |
| R04 | Focus: START/PAUSE/RESUME/RESET (CONFIRM); START rồi đi qua cả 6 app, quay lại | Như M1; thời gian vẫn đúng, không reset | `FOCUS: … state=RUNNING …` không bị đổi khi `APP_SWITCH` | PENDING | `src/services/FocusSessionService.cpp`; `focus_ui_tests`, `shell_tests S02` |
| R05 | Serial `CAL`, `TEST`, `DIAG`, `NEXT`, `HOME`, `BACK` | Như M1; CAL chỉ lưu khi thành công | `TOUCH_CAL: SAVED_TO_NVS` chỉ khi hoàn tất CAL | PENDING | `src/main.cpp` command() |
| R06 | Chạm 1 lần mỗi tile / nút; kéo ngón ra khỏi nút rồi nhả | Một chạm = một hành động; kéo ra = hủy | `TOUCH: EVENT TAP` một lần / chạm | PENDING | `src/core/AppManager.cpp`; `focus_ui_tests`, `mission_ui_tests MU1` |

## 2. M2 Daily Missions

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| D01 | Boot lần đầu sau khi nạp M2+ | 5 quest mặc định, DAY 1 | `MISSION_RESTORE: result=MISSING day=1`, `MISSION_STORE: SAVED slot=0 generation=1` | PENDING | `src/services/DailyMissionService.cpp` begin() |
| D02 | Tick STUDY SQL | Check + hiệu ứng, 20%, XP +10 | `MISSION_REWARD: key=mission:1:1 result=GRANTED` | PENDING | `mission_services_tests M02-M04` |
| D03 | Chạm lại nhiều lần / kéo / giữ trên quest đã tick | Không thêm XP, không ghi NVS | Không có `MISSION_STORE` mới | PENDING | `src/apps/DailyMissionsScreen.cpp`; `MU1`, `M05` |
| D04 | `<` / `>`, HOME/BACK, mở lại | Trạng thái giữ nguyên | — | PENDING | `MU2` |
| D05 | Chạm COMPLETE FOCUS; hoàn thành một phiên Focus | Báo "FINISH A 25:00 FOCUS"; sau phiên: +25 và quest tự tick +10 | `FOCUS: … xp=+25`, `MISSION_REWARD: … GRANTED` | PENDING | `src/services/FocusCompletionAdapter.h`; `M11` |
| D06 | Reset / rút nguồn | `result=LOADED`, cùng tick/XP; không thưởng lại | Không có `MISSION_REWARD` lúc boot | PENDING | `src/storage/MissionStorageNVS.cpp`; `M06`, `M12` |
| D07 | NEW DAY: chạm lặp → CANCEL; chờ 5 s → tự hủy; CONFIRM | DAY 2, 0/5, XP giữ | `MISSIONS: day=2 done=0/5 … total_xp=<không đổi>` | PENDING | `MU3`, `M09-M10` |
| D08 | `QTITLE 3 READ 20 PAGES`, reboot | Tên mới giữ sau reboot | `QTITLE: SAVED id=3` | PENDING | `M15` |

## 3. M3 Pixel Pet

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| P01 | Mở PET | BIT 96×96, LV theo XP (LV2 ở 100 XP, LV3 ở 300…), thanh tiến độ, XP = STATUS | `PET: level=… xp=…` | PENDING | `src/services/PetModel.cpp`; `pet_tests P01` |
| P02 | Chạm BIT | Mặt HAPPY + tim, PETS tăng; XP **không** đổi | `FOCUS`/`MISSIONS` không đổi xp | PENDING | `P02` |
| P03 | Để yên 45 s; chạm; nút SLEEP/WAKE | SLEEP + "Z" → tỉnh; nút hoạt động | — | PENDING | `src/services/PetService.cpp` |
| P04 | STYLE dưới / trên LV3; reboot | Báo "NEXT STYLE UNLOCKS AT LV 3"; đủ level đổi style, giữ sau reboot | `BLOB_STORE: SAVED ns=deskos-pet` | PENDING | `P03`, `P04` |
| P05 | Kiếm XP rồi mở PET | "+N XP!" hoặc "LEVEL UP!" | — | PENDING | `P02` |
| P06 | Quan sát 1 phút; HOME/BACK | Animation mượt, không nhấp nháy toàn màn | — | PENDING | `src/apps/PixelPetScreen.cpp`; `P05` |

## 4. M4 Pixel Window

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| W01 | Mở WINDOW (DAY) | Mây trôi, mặt trời, đồi; khung gỗ/thanh chia không bị vẽ đè | — | PENDING | `src/ui/WindowAnimator.cpp`; `window_tests W05` |
| W02 | Chạm kính / SCENE | NIGHT (sao, trăng), RAIN (mưa, chớp), AUTO | — | PENDING | `W02` |
| W03 | SPEED | SLOW/NORMAL/FAST khác biệt rõ | — | PENDING | `W04` |
| W04 | AUTO khi chưa có Wi-Fi | "NO CLOCK" + cảnh DAY (không đoán giờ) | `TIME: synced=0` | PENDING | `src/services/LiveWindowContext.h`; `W03` |
| W05 | Đổi scene/speed, rời app, reboot | Giữ nguyên | `WINDOW_RESTORE: load=1 scene=… speed=…`; `STATUS` → `WINDOW: …` | PENDING | `src/services/WindowSettings.cpp`; `W01` |

## 5. M5 Wi-Fi, NTP, HTTPS, Living Weather

Cấu hình (mạng 2.4 GHz; mật khẩu không được in lại):

```text
WIFI SSID TenWifiCuaBan
WIFI PASS matkhau-wifi
WIFI
```

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| N01 | Nhập Wi-Fi như trên | Kết nối, không treo UI | `NET: CONNECTING`, `NET: CONNECTED ip=…`, `TIME: SNTP_STARTED`, `HTTPS_TASK: STARTED stack=12288` | PENDING | `src/net/NetworkService.cpp` |
| N02 | `TIME` | Đúng giờ Việt Nam | `TIME: synced=1 local=HH:MM` | PENDING | `src/net/TimeServiceESP.cpp` |
| N03 | Chờ ≤ ~25 s sau khi có IP | WEATHER có nhiệt độ/điều kiện/độ ẩm/gió gần đúng; "UPDATED JUST NOW" | `HTTPS: code=200 bytes=… ms=… cert_date=VALID … heap=… max_block=… min_heap=…` | PENDING | `src/net/HttpsFetchTask.cpp`, `src/services/WeatherModel.cpp`; `WE1`, `WE7` |
| N04 | Trong lúc đang tải: chạm / đổi app | UI vẫn mượt | Không có `Task watchdog got triggered` | PENDING | HttpsFetchTask priority 0 core 0 |
| N05 | REFRESH hai lần trong 30 s | Lần 2: "PLEASE WAIT 30 S" | `WEATHER: BUSY_OR_RATE_LIMITED` (lệnh Serial) | PENDING | `WE5` |
| N06 | Tắt router / phát Wi-Fi; bật lại | UI không treo; WEATHER giữ dữ liệu cũ + "WIFI CONNECTING - OFFLINE"; tự kết nối lại và cập nhật | `NET: DISCONNECTED …`, rồi `NET: CONNECTED`, `HTTPS: code=200` | PENDING | `NetworkService::update`; `WE4` |
| N07 | Reboot khi không có mạng | "CACHED, AGE UNKNOWN" | `WEATHER: … has=1 src=1` | PENDING | `WE3` |
| N08 | Window AUTO khi đã có NTP | Ngày/đêm theo giờ thật; RAIN khi trời mưa | — | PENDING | `LiveWindowContext.h`; `W03` |
| N09 | (Tùy chọn) qua 00:00 với NTP | Missions sang ngày mới, XP giữ | `MISSIONS: day=+1 … total_xp=<giữ>` | PENDING | `M16` |
| N10 | `WEATHER LOC 21.0285 105.8542 HA NOI` | Lưu vị trí, dữ liệu Hà Nội | `WEATHER: LOCATION_SAVED` | PENDING | `WE5` |
| N11 | `WIFI OFF`; `WIFI ON` | Tắt Wi-Fi, app khác chạy bình thường; bật lại tự kết nối | `NET: state=OFF` → `NET: CONNECTED` | PENDING | `NetworkService::setEnabled` |
| N12 | Xem dòng `HTTPS:` đầu tiên sau boot có Wi-Fi | `cert_date=VALID` (nếu NTP đã sync trước fetch đầu) | Nếu `cert_date=DATE_UNCHECKED_NO_NTP` thì `TIME` lúc đó phải là `synced=0`; `EXPIRED/NOT_YET_VALID/BAD_DATE` = FAIL | PENDING | `HttpsFetchTask::run`, `TimeModel.h certDateStatus`; `WE7` |

## 6. M6 PC Monitor + Windows Python Agent

Làm theo [PC_AGENT_SETUP.md](PC_AGENT_SETUP.md).

| ID | Thao tác | Kỳ vọng | Serial cần thấy | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| C01 | Wi-Fi lên lần đầu; `PC TOKEN` | Token tạo một lần, in kèm lệnh agent | `PC: TOKEN_CREATED`, `PC: TOKEN …`, `PC: AGENT …` | PENDING | `PcMonitorService::ensureToken` |
| C02 | Chạy agent | PC MONITOR "CONNECTED", tên máy, CPU/RAM/DISK khớp Task Manager (±vài %), pin/mạng | `PC_LINK: LISTENING udp_port=47801`; `PC` → `accepted` tăng | PENDING | `src/net/PcLinkUDP.cpp`; `pc_tests PC3` |
| C03 | Ctrl+C agent / tắt laptop | ≤ 6 s → "DISCONNECTED", "PC AGENT STOPPED OR LAPTOP OFF" | — | PENDING | `PC4`, `PC5` |
| C04 | Agent với token sai | Không kết nối | `PC` → `last_reject=AUTH` | PENDING | `PC2` |
| C05 | `PC TOKEN NEW`; chạy agent cũ; cập nhật token | Agent cũ bị từ chối; token mới kết nối lại | `last_reject=AUTH` rồi `accepted` tăng | PENDING | `PC4`, `PC6` |
| C06 | Tắt Wi-Fi ESP32 | "NO WIFI" / "NOT LISTENING", không treo | `PC_LINK: STOPPED (Wi-Fi down)` | PENDING | `PcLinkUDP::update` |
| C07 | Agent chạy, NTP đã sync; `PC`; reset ESP32 (agent vẫn chạy) | Sau reboot agent kết nối lại ≤ 6 s (không bị khóa REPLAY) | Trước reset: `BLOB_STORE: SAVED ns=deskos-pc key=ctr bytes=24` (một lần), `PC: … replay_floor=1`; sau reset: `accepted` tăng, không có `last_reject=REPLAY` liên tục | PENDING | `PcMonitorService::begin/checkpointCounter`; `PC6` |

## 7. Ổn định và bộ nhớ (≥ 60 phút)

Chạy logger trong suốt bài test, `STATUS` tự gửi mỗi 10 phút:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\hw_validation\deskos_serial_log.py capture --port COM5 --minutes 70
```

| ID | Thao tác | Kỳ vọng | Serial / công cụ | Kết quả | Nếu FAIL: kiểm tra |
| --- | --- | --- | --- | --- | --- |
| S01 | ≥ 60 phút: Wi-Fi bật, agent chạy, một phiên Focus, nhiều lần chuyển app, Window FAST, ít nhất 4 lần fetch thời tiết | Không treo, không chậm dần | Summary: `uptime_max ≥ 60 min`, `https ok ≥ 4` | PENDING | — |
| S02 | Như S01 | Không reboot bất ngờ | Summary: `boots=1`, không có dòng `reset:` (rst:/Brownout/Guru/WDT) | PENDING | Nguồn USB (brownout); stack `HTTPS: stack_free`; `MEMORY loop_stack_free` |
| S03 | Như S01 | Heap không giảm liên tục; khối liên tục đủ cho TLS | Summary: `slope_after_warmup` không < −2048 B/h; `largest_block_min ≥ 45000`; `HTTPS … max_block` ổn định | PENDING | HttpsFetchTask, PcLinkUDP (WiFiUDP cấp phát mỗi gói), WeatherService |
| S04 | Cuối bài: `STATUS` | Calibration, XP, quests, pet, window, weather, PC đều đúng | Các dòng `TOUCH_CAL`, `FOCUS`, `MISSIONS`, `PET`, `WINDOW`, `WEATHER`, `PC`, `MEMORY` | PENDING | Module tương ứng |
| S05 | Đếm ghi NVS trong log | Không ghi NVS liên tục | `BLOB_STORE: SAVED … key=ctr` ≤ 5 lần/60 phút; `key=cache` ≤ 5; Focus checkpoint ≤ 1/phút khi chạy | PENDING | `PcMonitorService::FLOOR_SAVE_MS`, `WeatherService::REFRESH_MS` |

## 8. Điều kiện phát hành v1.0.0

Tất cả mục R/D/P/W/N/C/S ở trên **PASS** trên thiết bị thật (hoặc được người dùng
chấp nhận rõ ràng là ngoại lệ, ghi vào [KNOWN_ISSUES.md](KNOWN_ISSUES.md)), không có
FAIL mở. Gửi lại bảng + log (đã che secrets) để cập nhật
[DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) và [FINAL_QA_REPORT.md](FINAL_QA_REPORT.md).

Quay lại bản đã nghiệm thu: `git checkout m1-stable`, build, upload bình thường
(không erase). Namespace M2–M6 bị bỏ qua; XP Focus và calibration giữ nguyên.
