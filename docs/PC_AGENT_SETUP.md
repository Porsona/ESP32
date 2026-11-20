# PC Agent Setup (PC Monitor, DESKOS_1.0.0-RC1)

Agent `tools/pc_agent/deskos_agent.py` gửi CPU/RAM/DISK/mạng/pin từ laptop Windows
tới màn PC MONITOR của ESP32 qua UDP trong mạng LAN, xác thực bằng HMAC-SHA256
(giao thức DPM1).

## Đặc tính an toàn

- **Chỉ gửi**: agent không bind, không listen, không nhận gì; ESP32 không gửi lệnh
  nào về PC. Không cần mở cổng firewall trên PC.
- Chỉ gửi tới địa chỉ LAN (private); địa chỉ Internet bị từ chối.
- ESP32 chỉ nhận gói từ cùng subnet, kiểm HMAC trước khi đọc dữ liệu, chống replay.
  Từ RC1, counter lớn nhất đã nhận được lưu NVS (≤ 1 lần/15 phút, chỉ khi ESP32 có
  giờ NTP) nên gói cũ bắt được trên LAN không còn được chấp nhận lại sau khi ESP32
  reboot (trừ các gói của ≤ 15 phút cuối trước reboot, trước khi NTP đồng bộ).
- Dữ liệu **không mã hóa**: người cùng mạng LAN có thể đọc số liệu CPU/RAM.
- Token không nằm trong repo. Nó lấy từ `--token`, biến môi trường `DESKOS_TOKEN`
  hoặc `%APPDATA%\DeskOS\agent.json` (khi dùng `--save-config`).

## Yêu cầu

- Python 3.8+ trên Windows. Không cần thư viện ngoài.
- Tùy chọn: `pip install psutil` để có tốc độ mạng UP/DOWN; nếu không có, màn hình
  hiện `NET N/A` (CPU/RAM/DISK/pin vẫn có qua Win32 API).
- ESP32 đã kết nối Wi-Fi (xem [USER_GUIDE.md](USER_GUIDE.md), lệnh `WIFI`), cùng
  mạng với laptop.

## Các bước

1. Serial Monitor (115200), gõ `PC TOKEN`. ESP32 in:

   ```text
   PC: TOKEN 0123456789abcdef0123456789abcdef
   PC: AGENT python deskos_agent.py --host 192.168.1.50 --token 0123... --save-config
   ```

   Nếu báo `PC: CONNECT WIFI FIRST`, cấu hình Wi-Fi trước (token cần RNG phần cứng với RF bật).
2. Trên PC, từ thư mục project:

   ```powershell
   python tools/pc_agent/deskos_agent.py --host <IP ESP32> --token <TOKEN> --save-config
   ```

   `--save-config` lưu host/port/token vào `%APPDATA%\DeskOS\agent.json`; các lần
   sau chỉ cần `python tools/pc_agent/deskos_agent.py`.
3. Trên ESP32 mở PC MONITOR: dòng `LISTEN <ip> UDP 47801 (LAN ONLY)`, sau vài giây
   chuyển "CONNECTED" kèm tên máy và số liệu.
4. Dừng agent bằng Ctrl+C → sau ≤6 s màn hình báo "DISCONNECTED".

## Tùy chọn

| Tham số | Ý nghĩa |
| --- | --- |
| `--host` / `DESKOS_HOST` | IP của ESP32 (hiện trên màn PC MONITOR và lệnh `WIFI`) |
| `--port` / `DESKOS_PORT` | Cổng UDP, mặc định 47801 |
| `--token` / `DESKOS_TOKEN` | Token 32 ký tự hex từ `PC TOKEN` |
| `--interval` | Giây giữa hai gói, 1–60, mặc định 2 |
| `--once` | Gửi một gói rồi thoát |
| `--dry-run` | Chỉ in số liệu thu được, không gửi |
| `--save-config` | Lưu host/port/token vào `%APPDATA%\DeskOS\agent.json` |

## Đổi token / xử lý sự cố

- `PC TOKEN NEW` tạo token mới; agent cũ bị từ chối (`PC` → `last_reject=AUTH`).
  Chạy lại bước 2 với token mới.
- Lệnh `PC` in `link=… accepted=… rejected=… last_reject=…`:
  `AUTH` = sai token; `NOT_LAN` = PC khác subnet; `REPLAY` = counter cũ;
  `CLOCK_SKEW` = lệch giờ >300 s (kiểm đồng hồ Windows); `RATE_LIMIT` = quá 20 gói/s;
  `FORMAT` = gói sai định dạng; `NO_TOKEN` = ESP32 chưa có token.
- Không thấy gói nào (`accepted=0 rejected=0`): kiểm cùng Wi-Fi, router có bật
  "AP/client isolation" hay không, và IP ESP32 có đổi sau khi reboot router.
- IP ESP32 đổi → chạy lại agent với `--host <IP mới> --save-config`.
- `REPLAY` liên tục sau khi khởi động lại agent: đồng hồ Windows đã bị lùi (counter
  của agent là thời gian Unix tính bằng ms). Đồng bộ lại giờ Windows
  (Settings → Time → Sync now) hoặc `PC TOKEN NEW` rồi chạy agent với token mới.
- Không dán log Serial thô lên mạng: dòng `PC: TOKEN …` chứa token. Dùng
  `tools/hw_validation/deskos_serial_log.py` (tự che token), xem [USER_GUIDE.md](USER_GUIDE.md).
