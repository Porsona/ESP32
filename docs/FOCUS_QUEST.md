# Focus Quest — M1_FOCUS_01

2026-11-19. Build và automated host tests PASS; nghiệm thu M1 trên
ESP32-2432S028R **PENDING**. Chưa tự Upload hoặc truy cập NVS trên board.

## Sử dụng

Từ Home, chạm FOCUS:

| Thao tác/trạng thái | Kết quả |
| --- | --- |
| IDLE | FOCUS 25:00, tổng XP và số phiên đã hoàn thành |
| START | Chạy theo millis(); mỗi Focus mới có ID riêng |
| PAUSE | Lưu còn lại; thời gian tạm dừng không bị trừ |
| RESUME | Tiếp tục cùng ID và thời gian còn lại |
| RESET khi RUNNING | RESET → CONFIRM; CANCEL hoặc chờ 5 giây để hủy |
| RESET ở trạng thái khác | Focus IDLE 25:00, giữ XP/số phiên/ID đã dùng |
| Hết Focus | +25 XP và +1 phiên một lần; BREAK 05:00 chờ START |
| Hết Break | Không XP; FOCUS 25:00 chờ START |
| HOME/BACK/ứng dụng khác | Timer tiếp tục từ core |

Không tự chạy pha kế tiếp hoặc có nút Skip. Pause/Reset trước deadline không
thưởng XP. Nếu đã đủ 25 phút ngay khi nhận thao tác, completion được xử lý
trước. Một TAP hợp lệ kích hoạt một lần; kéo/giữ quá ngưỡng cũ hủy thao tác.
CAL/DIAG vẫn qua Serial và timer cũng chạy ở các view này.

## NVS và khôi phục

Record version 1, 40 byte, CRC32 chứa timer + XP + session IDs ở
`deskos-focus/state`. Calibration `cyd-touch/affine` không đổi. Ghi ở Start/
Pause/Resume/Reset/Complete; RUNNING checkpoint tối đa mỗi 60 giây.

Sau reset/cúp nguồn: RUNNING → PAUSED ở checkpoint, cần RESUME; không trừ
thời gian mất điện. PAUSED/IDLE/COMPLETED đã lưu giữ nguyên. COMPLETED chờ
START pha kế, không cộng XP khi boot.

Ghi bình thường có thể phải làm lại khoảng tối đa 60 giây công việc cộng độ
trễ loop/ghi. Nếu checkpoint ghi lỗi kéo dài, mất tiến độ có thể nhiều hơn.

SAVE PENDING - RETRYING: completion chưa được xác minh. START/RESET khóa,
retry cùng snapshot mỗi 5 giây. XP/count publish sau khi commit đã xác minh;
HOME/BACK vẫn dùng được. STORAGE_WRITE: giữ trạng thái RAM trước thao tác lỗi.
STORAGE_READ: khóa nút timer để không ghi đè tiến độ chưa đọc được; kiểm tra
lỗi rồi reboot để đọc lại.

INVALID_RESTORE: sai CRC/version/validation; fallback 25:00/0 XP và chưa ghi
đè tự động. START/RESET chủ động sẽ lưu record mới; không tự phục hồi XP từ
dữ liệu hỏng. Không erase NVS để thử, vì có map touch đã nghiệm thu.

## Build, test host và Upload

PlatformIO Terminal, tại project:

```powershell
pio run -e cyd_ili9341 -t clean
pio run -e cyd_ili9341
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run_focus_tests.ps1
```

Runner biên dịch C++ production với g++ trên PATH hoặc compiler riêng ở
`.pio/host-tools/toolchain-gccmingw32/bin/g++.exe`; có thể truyền `-Compiler`.
Compiler host không phải dependency firmware. Nếu cần cài lại, tạo thư mục
`.pio/host-tools`, rồi:

```powershell
pio pkg install --global --storage-dir .pio/host-tools --tool platformio/toolchain-gccmingw32 --no-save --skip-dependencies
```

Đóng Serial Monitor đang giữ COM5, người dùng tự nạp:

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

Serial cần có:

```text
FIRMWARE_VERSION: M1_FOCUS_01
DISPLAY_CONFIG: INVERT_ON
FOCUS_RESTORE: result=... running_restored_as_paused=... focus_ms=1500000 break_ms=300000
FOCUS: ... mode=FOCUS state=IDLE ...
SHELL_REGISTRY: READY ids=7 screen_instances=3
TOUCH_CAL: LOADED_FROM_NVS
DISPLAY_READY
```

Mode/state có thể khác IDLE nếu có tiến độ M1 trước đó. Restore result:
0=MISSING, 1=LOADED, 2=INVALID, 3=IO_ERROR. Gõ STATUS rồi Enter xem Focus và
calibration. HOME/BACK/CAL/RETRY/TEST/DIAG/NEXT/HELP giữ nguyên. Không có
command tua nhanh hoặc thời lượng production rút ngắn.

## Kiểm thử thiết bị

Chạy T01–T12 trong [TEST_REPORT_M1.md](TEST_REPORT_M1.md), đặc biệt:

1. Kiểm tra 25:00, Start/Pause/Resume, Reset/Confirm/Cancel và màu/font.
2. Chạy Focus → Home/placeholder/DIAG → Focus; thời gian vẫn giảm đúng.
3. Chờ đủ 25 phút: XP +25, sessions +1; nhiều lần ra/vào không thưởng lại.
4. START Break, chờ đủ 5 phút: không tăng XP; Focus 25:00 chờ START.
5. RUNNING có checkpoint SAVED → reset/rút nguồn: PAUSED từ checkpoint,
   không tính thời gian tắt nguồn. Reset sau completion SAVED không thưởng lại.
6. Thử sáu icon, HOME/BACK, TEST/DIAG và CAL khi muốn kiểm thử lại calibration.
   CAL chỉ thay map khi hoàn thành quy trình cũ.
7. Lưu MEMORY log 5 giây/lần, chạy ít nhất Focus+Break và nhiều transition;
   so heap sau warm-up, tìm reboot/xu hướng giảm liên tục bất thường.

Gửi kết quả từng Txx/log liên quan để cập nhật PASS/FAIL. Host tests không
thay thế touch, màu, nguồn/NVS và heap thực tế.

## Quay về M0

Commit `654b069`, tag `m0-stable`; source/config/BIN/ELF ở
`docs/baselines/M0_STABLE_before_M1_2026-11-19/`. Giữ thay đổi M1 trước khi
checkout tag hoặc dùng backup riêng; rebuild đúng baseline và upload bình
thường, không erase. M0 bỏ qua namespace Focus mới, calibration giữ nguyên.
Backup không phải bản export NVS từ board.
