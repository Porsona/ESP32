# Hardware Test Plan — M6_PCMON_01

Mục tiêu: nghiệm thu M2–M6 trên ESP32-2432S028R mà không làm mất dữ liệu đã có.
Agent không tự Upload; mọi mục dưới đây đang **PENDING** cho đến khi bạn gửi kết
quả/log. Ghi PASS/FAIL kèm dòng Serial liên quan.

## 0. Chuẩn bị

1. Đóng mọi Serial Monitor đang giữ COM5.
2. Ghi lại trạng thái cũ: mở monitor với firmware hiện tại, gõ `STATUS`, lưu log
   (XP, sessions, calibration). Không erase flash/NVS.
3. Nguồn USB tốt (cáp ngắn, cổng có đủ dòng): Wi-Fi tăng dòng tiêu thụ.
4. Upload (từ thư mục project):

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

## 1. Boot và hồi quy M0/M1 (làm trước tiên)

| ID | Thao tác → kết quả mong đợi | Kết quả |
| --- | --- | --- |
| R01 | Boot: `FIRMWARE_VERSION: M6_PCMON_01`, `DISPLAY_CONFIG: INVERT_ON`, `FOCUS_RESTORE: result=1`, `MISSION_RESTORE: result=…`, `PET: …`, `WINDOW_RESTORE: …`, `WEATHER: …`, `NET: state=NO CONFIG`, `SHELL_REGISTRY: READY ids=7 dedicated_apps=6 placeholders=0`, `TOUCH_CAL: LOADED_FROM_NVS`, `DISPLAY_READY` | PENDING |
| R02 | XP/sessions trong `FOCUS:` bằng giá trị trước khi nạp; không phải calibrate lại | PENDING |
| R03 | Home: 6 tile, màu/font đúng, banner `DESKOS M6` | PENDING |
| R04 | Focus: START/PAUSE/RESUME/RESET(CONFIRM) như M1; chạy Focus rồi đi qua cả 6 app, quay lại thời gian vẫn đúng | PENDING |
| R05 | `CAL`, `TEST`, `DIAG`, `NEXT`, `HOME`, `BACK` qua Serial như M1 | PENDING |

## 2. M2 Daily Missions

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| D01 | Lần đầu: `MISSION_RESTORE: result=MISSING day=1`, `MISSION_STORE: SAVED slot=0 generation=1` | PENDING |
| D02 | Tick STUDY SQL: check + hiệu ứng, 20%, XP +10, `MISSION_REWARD: key=mission:1:1 result=GRANTED` | PENDING |
| D03 | Chạm lại nhiều lần/kéo/giữ: không XP, không `MISSION_STORE` mới | PENDING |
| D04 | `<`/`>`, HOME/BACK, mở lại: trạng thái giữ | PENDING |
| D05 | Chạm COMPLETE FOCUS: "FINISH A 25:00 FOCUS". Hoàn thành một Focus: +25 và quest tự tick +10 | PENDING |
| D06 | Reset/rút nguồn: `result=LOADED`, cùng tick/XP, không `MISSION_REWARD` lúc boot | PENDING |
| D07 | NEW DAY: chạm lặp → CANCEL; 5 s → tự hủy; CONFIRM → DAY 2, 0/5, XP giữ | PENDING |
| D08 | `QTITLE 3 READ 20 PAGES` → SAVED, giữ sau reboot | PENDING |

## 3. M3 Pixel Pet

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| P01 | Mở PET: thấy BIT 96×96, LV = theo XP (LV2 ở 100 XP, LV3 ở 300…), thanh tiến độ, XP đúng bằng STATUS | PENDING |
| P02 | Chạm BIT: mặt HAPPY + tim, đếm PETS tăng; XP **không** đổi | PENDING |
| P03 | Để yên 45 s: SLEEP + "Z"; chạm → tỉnh; nút SLEEP/WAKE hoạt động | PENDING |
| P04 | STYLE: dưới LV3 báo "NEXT STYLE UNLOCKS AT LV 3"; đủ level thì đổi style, lưu sau reboot | PENDING |
| P05 | Kiếm XP (quest/focus) rồi mở PET: "+N XP!" hoặc "LEVEL UP!" | PENDING |
| P06 | Animation mượt, không nhấp nháy toàn màn; HOME/BACK | PENDING |

## 4. M4 Pixel Window

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| W01 | DAY: mây trôi, mặt trời có quầng sáng nhấp nháy, đồi; khung gỗ và thanh chia không bị vẽ đè | PENDING |
| W02 | Chạm khung/SCENE: NIGHT (sao lấp lánh, trăng khuyết), RAIN (mưa, chớp), AUTO | PENDING |
| W03 | SPEED: SLOW/NORMAL/FAST thay đổi tốc độ rõ rệt | PENDING |
| W04 | Chưa có Wi-Fi: AUTO hiển thị "NO CLOCK" và cảnh DAY (không đoán giờ) | PENDING |
| W05 | Rời app rồi reboot: scene/speed giữ (`WINDOW_RESTORE`) | PENDING |

## 5. M5 Wi-Fi, NTP, Living Weather

Trong Serial Monitor (thay bằng mạng 2.4 GHz của bạn; mật khẩu không được in lại):

```text
WIFI SSID TenWifiCuaBan
WIFI PASS matkhau-wifi
WIFI
```

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| N01 | `NET: CONNECTING`, sau đó `NET: CONNECTED ip=…`, `TIME: SNTP_STARTED`, `HTTPS_TASK: STARTED` | PENDING |
| N02 | `TIME` → `synced=1 local=HH:MM` đúng giờ Việt Nam | PENDING |
| N03 | ≤ ~25 s sau khi có IP: `HTTPS: code=200 bytes=… ms=… heap=…`; WEATHER hiện nhiệt độ/điều kiện/độ ẩm/gió gần đúng thực tế; "UPDATED JUST NOW" | PENDING |
| N04 | Trong lúc đang tải, chạm/đổi app vẫn mượt (UI không treo) | PENDING |
| N05 | REFRESH: lần 2 trong 30 s báo "PLEASE WAIT 30 S" | PENDING |
| N06 | Tắt router/phát Wi-Fi: UI không treo; WEATHER giữ dữ liệu cũ + "WIFI CONNECTING - OFFLINE"; bật lại → tự kết nối, tự cập nhật | PENDING |
| N07 | Reboot khi không có mạng: thời tiết cache hiển thị "CACHED, AGE UNKNOWN" | PENDING |
| N08 | Window AUTO khi đã có NTP: ngày/đêm theo giờ thật; RAIN khi trời mưa | PENDING |
| N09 | (Tùy chọn) qua 00:00 với NTP: Daily Missions sang ngày mới, XP giữ | PENDING |
| N10 | `WEATHER LOC 21.0285 105.8542 HA NOI` → LOCATION_SAVED, dữ liệu Hà Nội | PENDING |
| N11 | `WIFI OFF` → Wi-Fi tắt, app khác chạy bình thường; `WIFI ON` → kết nối lại | PENDING |

## 6. M6 PC Monitor

Làm theo [PC_AGENT_SETUP.md](PC_AGENT_SETUP.md): `PC TOKEN` trên Serial, chạy agent.

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| C01 | `PC: TOKEN_CREATED` khi có Wi-Fi lần đầu; `PC TOKEN` in token + lệnh agent | PENDING |
| C02 | Agent chạy: màn PC MONITOR "CONNECTED", tên máy, CPU/RAM/DISK khớp Task Manager (±vài %), pin/mạng | PENDING |
| C03 | Dừng agent (Ctrl+C) hoặc tắt laptop: ≤ 6 s → "DISCONNECTED", "PC AGENT STOPPED OR LAPTOP OFF" | PENDING |
| C04 | Agent với token sai: không kết nối; `PC` → `last_reject=AUTH` | PENDING |
| C05 | `PC TOKEN NEW` → agent cũ bị từ chối; cập nhật token → kết nối lại | PENDING |
| C06 | Tắt Wi-Fi ESP32: "NO WIFI"/"NOT LISTENING", không treo | PENDING |

## 7. Ổn định và bộ nhớ

| ID | Thao tác → kết quả | Kết quả |
| --- | --- | --- |
| S01 | ≥ 60 phút: Wi-Fi bật, agent chạy, một phiên Focus, nhiều lần chuyển app, Window FAST | PENDING |
| S02 | Không reboot bất ngờ (không có `rst:` / brownout trong log) | PENDING |
| S03 | `MEMORY … free_heap / largest_free_block` sau warm-up không giảm liên tục; `HTTPS … heap=` ổn định qua nhiều lần fetch | PENDING |
| S04 | Sau toàn bộ: `STATUS` → calibration, XP, quests, pet, window, weather, PC giữ đúng | PENDING |

Gửi lại bảng này (PASS/FAIL) cùng log Serial để cập nhật DEVELOPMENT_STATUS.md.
Muốn quay lại M1: `git checkout m1-stable`, build, upload bình thường (không erase);
các namespace M2–M6 bị bỏ qua, XP Focus và calibration giữ nguyên.
