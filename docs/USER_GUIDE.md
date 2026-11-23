# CYD DeskOS — User Guide (DESKOS_1.0.0-RC1)

ESP32-2432S028R, màn 320×240 cảm ứng. Serial 115200 baud (COM5 trên máy phát triển).

## Build và nạp

```powershell
pio run -e cyd_ili9341
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

Nạp thường **không** xóa NVS: calibration, XP, quest, pet, window, weather, Wi-Fi và
token PC đều được giữ. Không dùng "Erase Flash" nếu không muốn mất dữ liệu.

## Màn hình

Home có 6 tile; mọi app có BACK / HOME ở dải dưới (y=200..239).

| App | Chức năng | Tài liệu |
| --- | --- | --- |
| FOCUS | Pomodoro 25/5, +25 XP mỗi phiên hoàn thành | [FOCUS_QUEST.md](FOCUS_QUEST.md) |
| MISSIONS | 5 quest mỗi ngày, +10 XP mỗi quest, NEW DAY | [DAILY_MISSIONS.md](DAILY_MISSIONS.md) |
| PET | VOLT (robot mascot) lên level theo tổng XP; chạm đầu = xoa đầu, chạm bụng = cù, chạm hai bên = bắn móc cáp, vuốt = xoa, giữ ~1 s = treo ngược; style mở theo level | [PET_VOLT.md](PET_VOLT.md) |
| WINDOW | Cửa sổ pixel DAY/NIGHT/RAIN/AUTO, tốc độ SLOW/NORMAL/FAST | [TEST_REPORT_M4.md](TEST_REPORT_M4.md) |
| WEATHER | Thời tiết Open-Meteo, cập nhật 15 phút, cần Wi-Fi | [TEST_REPORT_M5.md](TEST_REPORT_M5.md) |
| PC MONITOR | CPU/RAM/DISK/mạng/pin từ laptop | [PC_AGENT_SETUP.md](PC_AGENT_SETUP.md) |

XP là dùng chung: XP Focus + XP Missions. Pet chỉ đọc XP, không tiêu XP.
Focus timer tiếp tục chạy khi chuyển sang app khác.

## Lệnh Serial

Gõ lệnh rồi Enter (newline). Lệnh không hợp lệ in danh sách lệnh.

| Lệnh | Tác dụng |
| --- | --- |
| `HOME`, `BACK`, `NEXT` | Điều hướng |
| `CAL` / `RETRY` | Calibrate cảm ứng (chỉ ghi NVS khi thành công) |
| `TEST`, `DIAG` | Màn thử cảm ứng / chẩn đoán |
| `STATUS` | In calibration, Focus, Missions, Pet, Window, mạng/NTP, thời tiết, PC Monitor, bộ nhớ |
| `MISSIONS`, `PET` | In trạng thái riêng |
| `QTITLE <id> <title>` | Đổi tên quest (1–16 ký tự A-Z 0-9 :+-/.!?#), giữ tick và XP |
| `WIFI SSID <tên>` | Lưu SSID (mạng 2.4 GHz) |
| `WIFI PASS <mật khẩu>` | Lưu mật khẩu 8–64 ký tự (không bao giờ in lại) |
| `WIFI OPEN` | Mạng không mật khẩu |
| `WIFI` | In trạng thái mạng (`pass_len`, IP, RSSI) |
| `WIFI ON` / `WIFI OFF` | Bật / tắt Wi-Fi |
| `WIFI FORGET` | Xóa SSID/mật khẩu đã lưu |
| `TIME` | Trạng thái NTP và giờ địa phương (UTC+7) |
| `WEATHER` | Trạng thái thời tiết |
| `WEATHER REFRESH` | Cập nhật ngay (tối thiểu 30 s giữa hai lần) |
| `WEATHER LOC <lat> <lon> [nhãn]` | Đổi vị trí, nhãn ≤15 ký tự |
| `PC` | Trạng thái PC Monitor (`link`, `token`, `accepted`, `rejected`, `last_reject`, `replay_floor`) |
| `PC TOKEN` / `PC TOKEN NEW` | In / tạo mới token cho agent |

Ví dụ cấu hình Wi-Fi lần đầu:

```text
WIFI SSID TenWifiCuaBan
WIFI PASS matkhau-wifi
WIFI
```

## Hành vi cần biết

- Khi đã có giờ NTP, Daily Missions tự sang ngày mới lúc 00:00 giờ Việt Nam;
  NEW DAY thủ công vẫn dùng được. XP không bao giờ bị reset.
- Không có Wi-Fi: mọi app vẫn chạy; WEATHER hiện dữ liệu cache, WINDOW AUTO hiện
  "NO CLOCK" và cảnh DAY, PC MONITOR báo "NO WIFI" / "NOT LISTENING - WIFI OFFLINE".
- Pet lưu số lần vuốt khi rời app; mất nguồn khi đang ở màn Pet làm mất các lần vuốt
  của phiên đó (XP không bị ảnh hưởng).

## Ghi log Serial để báo lỗi (che secrets)

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\hw_validation\deskos_serial_log.py capture --port COM5
```

- Log ghi vào `hw_logs\deskos_<ngày>_<giờ>.log` (không đưa vào Git), kèm file
  `.summary.json`. Token PC, `--token …`, `WIFI PASS …` và SSID được thay bằng
  `<REDACTED>` trước khi ghi; thêm `--keep-ssid` nếu muốn giữ SSID.
- Mặc định không reset board khi mở cổng; `--reset` để bắt log boot; `--minutes 70`
  để tự dừng; logger tự gửi `STATUS` mỗi 10 phút (`--status-every 0` để tắt).
- Phân tích lại một log: `… deskos_serial_log.py analyze hw_logs\<file>.log`
  (đếm boot/reset/brownout, heap thấp nhất, xu hướng heap, HTTPS, lỗi NVS). Công cụ
  chỉ cảnh báo tự động, không thay cho việc bạn quan sát màn hình.
- Log của `pio device monitor` có thể chứa token: dùng
  `analyze <log> --redact-to <bản-chia-sẻ>.log` trước khi gửi.

## Quay lại bản cũ

`git checkout m1-stable`, build và nạp bình thường (không erase). Các namespace
M2–M6 bị bỏ qua; XP Focus và calibration giữ nguyên.
