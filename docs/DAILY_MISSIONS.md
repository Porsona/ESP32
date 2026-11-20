# Daily Missions — M2_MISSIONS_01

2026-11-20. Build và automated host tests PASS; nghiệm thu trên
ESP32-2432S028R **PENDING**. Agent chưa Upload, chưa truy cập NVS trên board.

## Sử dụng

Từ Home, chạm MISSIONS.

```text
y=0..31    DESKOS M2                                   DAILY QUESTS
y=32..57   DAY 1        2/5 DONE  PAGE 1/2           XP 135
           [=========================                ]
y=58..199  [x] STUDY SQL               DONE +10 XP   [ < ]
           [ ] PRACTICE PYTHON   TAP TO COMPLETE +10 [ > ]
           [ ] READ 10 PAGES     TAP TO COMPLETE +10 [NEW DAY]
y=200..239 [        BACK        ]  [        HOME        ]
```

| Thao tác | Kết quả |
| --- | --- |
| Chạm một hàng quest chưa xong | Tick, hiệu ứng pixel, +10 XP vào XP dùng chung, lưu NVS |
| Chạm quest đã xong | "DONE - XP CLAIMED"; chỉ xem, không bỏ tick, không thưởng lại |
| Chạm COMPLETE FOCUS | "FINISH A 25:00 FOCUS"; chỉ tự hoàn thành khi phiên Focus 25 phút kết thúc |
| `<` / `>` | Trang 1 (quest 1–3) / trang 2 (quest 4–5); không đổi dữ liệu |
| NEW DAY | Hộp thoại "START DAY N+1?"; CONFIRM (trái) đổi ngày, CANCEL (phải) hủy |
| HOME / BACK | Về Home; quest, trang và Focus timer giữ nguyên |

Thanh tiến độ: 0/5, 1/5 … 5/5 = 0%, 20% … 100%. Tổng XP góc phải là
`ProgressService::totalXP()` = XP Focus + XP Missions; Focus Quest cũng hiện số này.

Chống thao tác nhầm: một TAP hợp lệ cho một hành động (cổng PRESS→RELEASE→TAP
của AppManager; kéo >8 px hoặc giữ >700 ms bị hủy). CONFIRM chỉ nhận sau 300 ms
và ở vị trí khác NEW DAY; hộp thoại tự hủy sau 5 s hoặc khi rời màn hình; NEW DAY
khóa 3 s sau khi đổi ngày; 500 ms sau khi đóng hộp thoại bỏ qua chạm xuống
hàng quest bên dưới.

## Quest template

| questId | Title | XP | Loại |
| --- | --- | --- | --- |
| 1 | STUDY SQL | 10 | Manual |
| 2 | PRACTICE PYTHON | 10 | Manual |
| 3 | READ 10 PAGES | 10 | Manual |
| 4 | REVIEW NOTES | 10 | Manual |
| 5 | COMPLETE FOCUS | 10 | Focus (tự động) |

Title tối đa 16 ký tự `A-Z 0-9 space : + - / . ! ? #` (16 × 12 px = 192 px ở
GLCD size 2). Đổi title qua Serial (không có bàn phím ảo):

```text
QTITLE 3 READ 20 PAGES
```

Đổi title giữ questId, trạng thái và reward key; không cấp thưởng mới.

## Ngày nhiệm vụ

- `dayId` bắt đầu 1, tăng 1 mỗi lần NEW DAY được xác nhận. M2 không có NTP nên
  không bao giờ tự reset theo millis() hoặc đoán ngày.
- NEW DAY: bỏ tick 5 quest, mở reward key của ngày mới, neo lại mốc Focus.
  Giữ tổng XP, lịch sử thưởng (số lần thưởng, các ngày cũ đóng vĩnh viễn),
  title, Focus timer/XP, calibration; không đụng Pixel Pet.
- Mỗi xác nhận mang `dayId` lúc mở hộp thoại; service từ chối nếu ngày đã đổi
  (`STALE_REQUEST`), nên một lần chạm không thể tăng hai ngày.
- M5: một `IDaySource` (TimeService/NTP) có thể truyền khóa ngày lịch; service
  ghi dấu lần đầu, mở ngày mới khi khóa tăng, bỏ qua khi đồng hồ lùi.

## XP và chống thưởng trùng

- Reward key `mission:<dayId>:<questId>`, ví dụ `mission:3:2`.
- Thưởng qua `ProgressService::grantRewardOnce()` trên candidate; ledger trong
  ProgressService chỉ cập nhật sau khi NVS xác minh.
- Mở app, đổi trang, render, boot: không thưởng, không ghi.
- Chạm lặp: quest đã completed trả về `ALREADY_COMPLETED`, không ghi NVS.
- Không có bỏ tick nên không có vòng tick/bỏ tick để thưởng nhiều lần.
- Ngày cũ đóng (`DAY_CLOSED`); cùng questId ở ngày mới là key mới, được thưởng lại.
- Focus Quest vẫn +25 XP/phiên theo cơ chế M1 (record Focus không đổi).

## Tích hợp Focus Quest

M1 không có EventBus, nên dùng adapter nhỏ `FocusCompletionAdapter`, chỉ đọc:
`lastRewardedSessionId` của ProgressService chỉ tăng khi completion +25 XP đã
commit. Khi giá trị này lớn hơn `focusAnchorId` (mốc lúc mở ngày) và quest
COMPLETE FOCUS chưa xong → hoàn thành quest + 10 XP trong cùng vòng loop.

- Mở Focus Quest, START rồi RESET, Pause/Resume, Break: không hoàn thành.
- Phiên Focus thứ hai cùng ngày: chỉ +25.
- Phiên kết thúc trước NEW DAY không tính cho ngày mới.
- Reboot sau khi Focus đã lưu +25 nhưng trước khi mission lưu +10: anchor đã
  lưu nên update đầu tiên sau boot phục hồi +10 đúng một lần.

## NVS và khôi phục

Namespace `deskos-quest`, key `slot0`/`slot1`, record version 1, 168 byte,
CRC32. Không sửa `cyd-touch` hoặc `deskos-focus`, không erase, không đổi
partition table.

| Tình huống | Hành vi |
| --- | --- |
| Lần đầu (MISSING) | Template ngày 1, ghi một lần để chốt mốc Focus |
| LOADED | Khôi phục ngày, tick, title, XP mission |
| RECOVERED_PREVIOUS_SLOT | Slot mới nhất hỏng; dùng commit trước đó |
| INVALID (mọi slot hỏng/sai version) | Mặc định ngày 1, 0 XP mission; không tự ghi đè đến khi người dùng thao tác |
| IO_ERROR | Khóa tick/NEW DAY/QTITLE đến khi reboot đọc được; không đè dữ liệu chưa đọc |
| Ghi lỗi hoặc không chắc chắn | SAVE PENDING, retry đúng bản ghi đó mỗi 5 s; khóa thao tác mission; XP hiện sau khi commit |

Completion và reward nằm trong cùng blob nên mất nguồn chỉ để lại trạng thái cũ
(chưa tick, chưa XP) hoặc trạng thái mới (đã tick, đã XP), không có trạng thái
nửa vời. Mỗi slot ghi được đọc lại và so byte.

## Serial

| Lệnh | Tác dụng |
| --- | --- |
| `STATUS` | Calibration + FOCUS + MISSIONS + QUEST |
| `MISSIONS` | MISSIONS + 5 dòng QUEST |
| `QTITLE <id> <title>` | Đổi title quest |
| HOME/BACK/CAL/RETRY/TEST/DIAG/NEXT/HELP | Như M1 |

```text
MISSION_RESTORE: result=LOADED day=3 bonus_xp=120 total_xp=245
MISSIONS: ms=... day=3 done=2/5 focus_anchor=5 rewards=12 bonus_xp=120 total_xp=245 generation=19 save_pending=0 error=NONE
MISSION_REWARD: key=mission:3:2 result=GRANTED bonus_xp=130 total_xp=255
MISSION_STORE: SAVED slot=1 generation=20
QUEST: id=2 title="PRACTICE PYTHON" kind=MANUAL xp=10 done=1 rewarded=1
```

`focus_anchor=-1`: chưa biết mốc Focus (Focus storage lỗi đọc); sẽ tự neo
khi Focus đọc được, không tự hoàn thành.

## Build, test và Upload

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_mission_tests.ps1
pio run -e cyd_ili9341 -t clean
pio run -e cyd_ili9341
```

`run_mission_tests.ps1` chạy test service và UI của M2, rồi chạy lại toàn bộ
suite M1. Người dùng đóng Serial Monitor đang giữ COM5 rồi tự nạp:

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

Upload thường, không erase: calibration, XP Focus và timer M1 được giữ.
Kiểm thử phần cứng H01–H13 trong [TEST_REPORT_M2.md](TEST_REPORT_M2.md).

## Quay về M1

`git checkout m1-stable`, build và upload. M1 bỏ qua namespace `deskos-quest`;
XP Focus và calibration giữ nguyên, XP Missions không hiển thị (vẫn nằm trong
NVS, quay lại M2 sẽ thấy lại).
