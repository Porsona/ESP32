# CYD DeskOS — TEST_REPORT_M0

Ngày cập nhật: 2026-11-19 (Asia/Saigon).

**M0 đã được người dùng nghiệm thu trước M1: M0.0/M0.1/M0.2 PASS; M0.3 Home/AppManager PASS, 8/8 test theo báo cáo người dùng. Các bảng chi tiết bên dưới lưu lịch sử lúc triển khai, không phải phép đo mới do agent chạy. Xem [M0_ACCEPTANCE.md](M0_ACCEPTANCE.md); M1 sẽ có TEST_REPORT_M1.md riêng.**

## Baseline phần cứng đã xác nhận

- Board ESP32-2432S028R, ESP32-D0WD-V3; landscape 320 × 240.
- ILI9341_2_DRIVER, `tft.setRotation(1)`, `tft.invertDisplay(true)`.
- XPT2046 trên SPI riêng; Serial COM5, 115200 baud.
- FINAL_04 đã vượt qua toàn bộ kiểm tra màu theo xác nhận người dùng trước M0.1: RGB/BLACK/WHITE và nền đúng ngay sau boot, sau 30 giây, các lần RESET và ngắt/cắm nguồn. **Color PASS áp dụng cho FINAL_04**, không tự chuyển sang bản font mới.
- Lịch sử A/B từng báo ngược inversion; thử nghiệm ba trạng thái sau đó và FINAL_04 trên board xác nhận true đúng màu. Tiếp tục giữ true.

Đã sao lưu bản ổn định trước thay đổi tại [baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19](baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19/): source, cấu hình, pin header, firmware BIN/ELF, bootloader, partitions, boot_app0, log, báo cáo cũ và xác nhận board. `SHA256.json` ghi hash; đã kiểm tra lại bản sao sau build mới.

SHA-256 firmware FINAL_04 được lưu:

```text
3FD106A676E5F1CA0DC4348EBDA548BCD892FD7F650A5489E940D58F0EC9E299
```

## Giai đoạn A — thay đổi

- `platformio.ini`: thêm duy nhất `-DLOAD_GLCD=1` trong build_flags, giữ nguyên tất cả dòng cũ. Không bật TFT_INVERSION_ON/OFF; dòng gợi ý cuối file chỉ là chú thích.
- `src/main.cpp`: version `M0_FONT_TEST_01`, chọn rõ `tft.setTextFont(1)` trong hàm vẽ. Font GLCD 1 dùng cỡ chữ 2 cho nội dung kiểm tra và cỡ 1 cho phiên bản.
- Nền `TFT_BLACK`, chữ `TFT_WHITE`; hiển thị `CYD DESKOS`, `DISPLAY TEST M0.1`, `RED`, `GREEN`, `BLUE`, `ABC 123`, phiên bản. Ba ô từ trái sang phải dùng `TFT_RED`, `TFT_GREEN`, `TFT_BLUE`.
- Mọi phần tử được bố trí trong 320 × 240 với lề: title ở (16,16), subtitle ở (16,44); ô RGB tại x=20/115/210, y=94, rộng 89, cao 56; nhãn ở y=162; `ABC 123` tại (16,192); phiên bản ở (16,222). Chưa xác nhận trực quan trên board.
- Giữ nguyên setup của FINAL_04, chỉ thay hàm vẽ: init TFT → rotation 1 → text wrap false → khởi tạo touch → log → inversion true một lần → vẽ → DISPLAY_READY. Không thêm lời gọi init/rotation/inversion khác.
- Vòng lặp đọc raw touch giữ nguyên, không chuyển trang hoặc vẽ lại theo frame. Không thêm framebuffer.
- Không đổi driver, GPIO, framework, dependency, RGB order, gamma. Chưa tách HAL, chưa làm DiagnosticScreen, chưa chạy soak test 15 phút hoặc triển khai ứng dụng DeskOS.

## Build và kiểm tra trên máy

| Kiểm tra | Trạng thái | Bằng chứng |
| --- | --- | --- |
| Sao lưu FINAL_04 | PASS | Hash nguồn/bản sao khớp; manifest còn nguyên sau build |
| Giữ build flags cũ | PASS | Bỏ đúng dòng LOAD_GLCD mới thì platformio.ini khớp baseline |
| Giữ GPIO | PASS | SHA-256 board_pins.h không đổi |
| Giữ setup/touch loop | PASS | Setup khớp baseline ngoài tên hàm vẽ và chú thích; loop không đổi |
| Clean cyd_ili9341 | PASS | Exit code 0, 0.50 giây; [log Clean](M0_FONT_TEST_01_clean.log) |
| Build cyd_ili9341 | PASS | Exit code 0, 14.41 giây; [log Build](M0_FONT_TEST_01_build.log) |
| Upload bản font | PENDING | Không tự động Upload |

Lệnh thực tế: `pio run -e cyd_ili9341 -t clean`, sau đó `pio run -e cyd_ili9341`, dùng PlatformIO cài sẵn tại `C:\Users\Acer\.platformio\penv\Scripts\platformio.exe`.

RAM: 22,260 / 327,680 bytes (6.8%). Flash: 287,433 / 1,310,720 bytes (21.9%). Cảnh báo TFT_eSPI `TOUCH_CS pin not defined` vẫn như trước; touch dùng thư viện XPT2046 riêng, không đổi cấu hình để xử lý cảnh báo trong bước này.

Đã kiểm tra BIN chứa version và các chuỗi thử mới; không còn version FINAL_04. Kiểm tra này không thay thế xác nhận font/màu trên màn hình thật.

SHA-256 `.pio/build/cyd_ili9341/firmware.bin` của M0_FONT_TEST_01:

```text
301BA64C407F7AA86E0C5A50B208037FACF22088C32AAE861A1D0E6ECB6F1B3A
```

## Upload và xác nhận giai đoạn A (lịch sử)

Người dùng đã nạp M0_FONT_TEST_01 và xác nhận RGB/nền đen/font GLCD đúng sau RESET, đồng thời yêu cầu tiếp tục M0.1B. Quy trình dưới đây lưu lịch sử của A; project hiện tại là bản B. Để nạp firmware mới, dùng hướng dẫn M0.1B bên dưới.

1. Đóng Serial Monitor để giải phóng COM5.
2. Trong PlatformIO Terminal ở thư mục project:

   ```powershell
   pio run -e cyd_ili9341 -t upload --upload-port COM5
   ```

3. Sau Upload SUCCESS:

   ```powershell
   pio device monitor -p COM5 -b 115200
   ```

4. Nhấn EN/RESET nếu cần thu lại log từ đầu. Xác nhận đúng các marker kỳ vọng, có thể xen log touch init/heap:

   ```text
   FIRMWARE_VERSION: M0_FONT_TEST_01
   DISPLAY_CONFIG: INVERT_ON
   DISPLAY_READY
   ```

5. Kiểm tra đủ chữ và không mất ký tự ở `CYD DESKOS`, `DISPLAY TEST M0.1`, `RED`, `GREEN`, `BLUE`, `ABC 123`. RGB phải đúng thứ tự trái sang phải, nền đen. Quan sát chữ không bị cắt ở mép; chạm vẫn có raw log và không đổi trang.
6. Gửi kết quả font và màu trên board. **Dừng ở giai đoạn A; chỉ thực hiện giai đoạn B sau khi người dùng xác nhận A PASS.**

Nếu `pio` chưa nằm trong PATH, thay bằng `& 'C:\Users\Acer\.platformio\penv\Scripts\platformio.exe'` trong các lệnh PowerShell trên.

| Kiểm tra trên M0_FONT_TEST_01 | Trạng thái | Bằng chứng |
| --- | --- | --- |
| Upload M0_FONT_TEST_01 | PASS | Người dùng xác nhận đã nạp |
| Font GLCD đúng sau RESET | PASS | Người dùng xác nhận trên board |
| RGB đúng và nền đen sau RESET | PASS | Người dùng xác nhận trên board |
| Chữ/ô màu không bị cắt ở mép | PENDING | Chờ màn hình thật |
| Raw touch vẫn hoạt động | PENDING | Chờ người dùng |
| Giai đoạn A được chấp nhận | PASS | Người dùng xác nhận font/màu và yêu cầu tiếp tục B |

## Giai đoạn B — M0_1B_DISPLAY_DIAG_02 (đã được người dùng nghiệm thu)

Nguồn xác nhận mới: yêu cầu M0.2 đính kèm của người dùng, ghi rõ milestone M0.1B
PASS trên phần cứng thật: Build/Upload, RGB/BLACK/WHITE, font, geometry/bốn góc,
System Info, touch chuyển trang và hoạt động ổn định 15 phút. Đây là kết quả do
người dùng cung cấp; agent không tự chạy board hoặc đo lại heap. Các hướng dẫn
B bên dưới lưu lịch sử, không phải hướng dẫn nạp source M0.2 hiện tại.

### Sao lưu trước thay đổi

Đã lưu bản A đã được chấp nhận tại [baselines/M0_FONT_TEST_01_verified_before_M0_1B_2026-11-19](baselines/M0_FONT_TEST_01_verified_before_M0_1B_2026-11-19/): source, platformio.ini, board_pins.h, BIN/ELF, bootloader, partitions, boot_app0, log Clean/Build, các báo cáo trước sửa, SHA256.json và BOARD_CONFIRMATION.md. Firmware khớp SHA-256 `301BA64C407F7AA86E0C5A50B208037FACF22088C32AAE861A1D0E6ECB6F1B3A` của bản font ở trên. Đã kiểm tra lại toàn bộ manifest sau build B, không thay đổi.

Bản sao lưu ổn định đã có từ lần triển khai B trước, được kiểm tra lại toàn bộ hash trong lần cập nhật này và tiếp tục giữ nguyên; không dùng source B để ghi đè baseline M0_FONT_TEST_01. Bản B _02 bổ sung ký tự đặc biệt và tên chế độ COLORS; chưa có bằng chứng board cho bản B _01 hoặc _02.

### Cấu trúc project và khởi tạo

```text
include/DisplayHAL.h       API và đối tượng TFT duy nhất
src/DisplayHAL.cpp         backlight, TFT init/rotation, inversion
include/DiagnosticScreen.h trạng thái test, lịch cập nhật, số liệu
src/DiagnosticScreen.cpp   bốn trang test, render và log
src/main.cpp               setup, touch, điều phối diagnostics
include/board_pins.h       pin touch/backlight hiện có, không đổi
platformio.ini            TFT pins/driver/HSPI/font/dependency, không đổi
```

- `DisplayHAL::begin()` giữ backlight OUTPUT/HIGH → TFT init → rotation 1 → text wrap false. Main tiếp tục khởi tạo touch VSPI và rotation 1, in log, rồi `DisplayHAL::applyColorConfig()` áp dụng true trước lần vẽ đầu. Hai hàm có guard để không khởi tạo hoặc gửi inversion lại khi vô tình gọi nhiều lần.
- Chỉ có một đối tượng TFT và một vị trí gọi inversion true trong HAL. DiagnosticScreen không gọi init/rotation/inversion. Không có inversion false.
- Giữ nguyên ILI9341_2_DRIVER, HSPI của TFT, VSPI/pin XPT2046, LOAD_GLCD=1, framework và dependency. `platformio.ini` và `board_pins.h` khớp SHA-256 bản A.
- TFT pins vẫn ở build_flags; touch/backlight vẫn ở board_pins.h. Backlight 21 đã trùng khớp từ trước ở cả hai cấu hình; thêm `static_assert(PIN_BACKLIGHT == TFT_BL)` để chặn mâu thuẫn lúc build, không thêm số GPIO ở main/HAL.
- Touch vẫn dùng `touch.touched()`, `touch.getPoint()` và raw log mỗi 120 ms khi chạm. Thêm chuyển trang theo cạnh nhấn, khoảng chặn 300 ms; giữ một ngón không chuyển liên tục, phải nhả rồi chạm lại. Không dùng hoặc hiệu chuẩn tọa độ cho điều hướng.
- Không có full-screen framebuffer, sprite hoặc cấp phát động tường minh trong mã mới. Dùng các buffer ký tự nhỏ trên stack và font GLCD 1 hiện có.

### Các trang

| Thứ tự | Trang | Nội dung và cách đối chiếu |
| --- | --- | --- |
| 1 | COLORS | Năm ô RED, GREEN, BLUE, BLACK, WHITE; nền đen, ô BLACK có viền trắng |
| 2 | FONT | CYD DESKOS, ABC 123, A–Z/a–z/0–9, ký tự đặc biệt; font 1 size 1/2; LEFT/CENTER/RIGHT theo các đường mốc |
| 3 | GEOMETRY | Viền tại x=0/319, y=0/239; bốn dấu L nằm trong viền; nhãn TL/TR/BL/BR; tâm (160,120), chữ thập và vòng tròn; dòng 320x240 LANDSCAPE |
| 4 | SYSTEM | Free heap, largest block, min free, uptime, previous/peak render, draw count, baseline heap |

Mỗi chạm mới chuyển COLORS → FONT → GEOMETRY → SYSTEM → COLORS. Header cho biết trang đang chạy. Footer hiển thị uptime, tiến độ 15 phút và `HW CHECK: PENDING`.

FONT có thêm dòng `!@#$%^&*()_+-=[]{};:,.?/` ở (12,182), dùng font GLCD 1 size 1; giữ dòng alignment và mô tả font riêng, không chồng lên footer. Ký tự đặc biệt ở đây là dấu ASCII in được, chưa được kiểm tra trực tiếp trên board của bản B.

### Lịch cập nhật và ý nghĩa số liệu

- Dùng `millis()` với phép trừ unsigned. Lấy mẫu heap và cập nhật footer mỗi 1000 ms trên mọi trang; SYSTEM đồng thời vẽ lại vùng số liệu. Những vùng khác giữ nguyên; đặc biệt không xóa viền/góc Geometry. Chỉ giữ `delay(15)` ngắn từ bản A, không có delay dài hoặc vòng đợi 15 phút.
- Log `DIAG:` khi bắt đầu, khi đổi test và mỗi 5000 ms. Dòng có version, trang, kích thước, `uptime_ms`, `free_heap`, `largest_free_block`, `baseline_heap`, `heap_delta`, `min_free_heap`, `min_largest_block`, `render_us`, `peak_render_us`, `draw_count`, `soak`.
- `ESP.getFreeHeap()` và `ESP.getMaxAllocHeap()` cùng dùng MALLOC_CAP_INTERNAL trong Arduino ESP32 2.0.11 đang cài. Largest block là khối cấp phát liên tục lớn nhất của cùng pool heap, không phải tổng free heap.
- Baseline heap lấy trước lần vẽ diagnostics đầu tiên sau khởi tạo. Min heap/block là mức thấp nhất đã lấy mẫu trong phiên. `heap_delta` so với baseline có thể âm; một lần giảm hoặc chỉ số min giảm không tự chứng minh leak. Cần đối chiếu mẫu hiện tại qua thời gian và lặp cùng chuỗi trang.
- Thời gian render đo bằng `micros()` quanh thao tác vẽ, đơn vị microsecond; không tính lấy mẫu heap và Serial log. Có thể là vẽ cả trang hoặc vùng cập nhật. Trên SYSTEM, `Prev draw` là lần vẽ đã hoàn tất trước đó; Serial ghi số đo mới sau khi vẽ. `draw_count` gồm cả cập nhật footer, không phải FPS.
- Mốc 15 phút tính từ `diagnostics.begin()` sau khởi tạo; đổi trang không reset bộ đếm. Khi đủ 900000 ms, Serial báo `soak=ELAPSED_PENDING_REVIEW`; màn hình báo `15m: ELAPSED`. **Không tự đánh dấu PASS** vì vẫn cần xác nhận không reboot, màu/font/geometry và xem heap trend. Reset board sẽ bắt đầu phiên mới và in version/BEGIN lại.

### Kết quả build B trên máy

| Kiểm tra | Trạng thái | Bằng chứng |
| --- | --- | --- |
| Backup bản A | PASS | Toàn bộ SHA-256 manifest còn nguyên sau build B |
| Giữ cấu hình/pins/dependency | PASS | platformio.ini và board_pins.h khớp baseline; dependency graph không đổi |
| Trình tự init và inversion | PASS cho kiểm tra mã | Giữ TFT → touch → log → inversion true → vẽ; một vị trí inversion, không có false |
| Không cấp phát framebuffer | PASS cho kiểm tra mã | Không dùng TFT_eSprite/createSprite, malloc/new trong source mới |
| Clean cyd_ili9341 | PASS | Exit code 0, 0.64 giây; [log Clean B _02](M0_1B_DISPLAY_DIAG_02_clean.log) |
| Build cyd_ili9341 | PASS | Exit code 0, 11.24 giây; [log Build B _02](M0_1B_DISPLAY_DIAG_02_build.log) |
| Upload M0.1B | PASS theo người dùng | Người dùng xác nhận trước M0.2 |

RAM: 22,332 / 327,680 bytes (6.8%); Flash: 292,361 / 1,310,720 bytes (22.3%). Cảnh báo TFT_eSPI `TOUCH_CS pin not defined` vẫn như trước; không bật touch tích hợp của TFT_eSPI vì project dùng XPT2046 riêng.

Đã kiểm tra BIN có version, bốn tên bài test và các trường log bộ nhớ/render/soak mới, không còn version M0_FONT_TEST_01. Đây là xác nhận artifact trên máy, không phải kết quả board.

SHA-256 firmware B đã được sao lưu trước M0.2 (không còn là BIN hiện tại):

```text
15319FDF1B45CF0246696816EE896A8249562E304D4123669FEA36926F6E83BC
```

### Upload và kiểm thử trên board B (quy trình lịch sử)

Project hiện tại là M0.2; dùng quy trình trong [TOUCH_CALIBRATION.md](TOUCH_CALIBRATION.md)
để nạp thử touch calibration. Chỉ dùng hướng dẫn B sau khi khôi phục source B từ backup.

1. Đóng Serial Monitor để giải phóng COM5. Trong PlatformIO Terminal ở thư mục project, nạp thủ công:

   ```powershell
   pio run -e cyd_ili9341 -t upload --upload-port COM5
   ```

2. Mở lại Serial Monitor 115200, giữ/lưu log suốt phiên 15 phút:

   ```powershell
   pio device monitor -p COM5 -b 115200
   ```

3. Nhấn EN/RESET để bắt đầu một phiên từ đầu. Các marker kỳ vọng, không phải log đã thu trên board:

   ```text
   FIRMWARE_VERSION: M0_1B_DISPLAY_DIAG_02
   DeskOS hardware test | touch init: OK
   DISPLAY_CONFIG: INVERT_ON
   MEMORY_CAPS: INTERNAL | SAMPLE_MS: 1000 | LOG_MS: 5000
   DIAG: event=BEGIN ... test=COLORS size=320x240 ... soak=RUNNING
   DISPLAY_READY
   ```

4. COLORS: kiểm tra đúng năm màu và nền đen. Chạm/nhả từng lần để kiểm tra FONT, GEOMETRY, SYSTEM rồi quay về COLORS. Giữ ngón vài giây: chỉ đổi một trang; raw touch vẫn in. Không cần tọa độ chạm chính xác.
5. FONT: đủ chữ/số/ký tự đặc biệt, size 1/2 rõ; LEFT/CENTER/RIGHT nằm đúng mốc, không cắt mép. GEOMETRY: thấy trọn bốn cạnh, bốn dấu góc, tâm ở giữa và kích thước 320x240. Quan sát cả khi footer cập nhật để chắc chắn góc/viền không bị xóa.
6. SYSTEM: free/largest >0, largest không vượt tổng free, uptime và draw count tăng, previous/peak render có số đo. Kiểm tra log có PERIODIC khoảng 5 giây và TEST_CHANGE tương ứng thao tác.
7. Chạy liên tục ít nhất 15 phút trong cùng một phiên không reset. Trong vài phút đầu, lặp nhiều vòng đổi trang; sau đó ở SYSTEM để theo dõi, và đổi một vòng nữa trước khi kết thúc. So sánh heap tại cùng trang/chuỗi thao tác sau warm-up, khoảng phút 1, 5, 10, 15; giữ cả log đầu và cuối. Không có boot marker lặp lại, uptime không về 0, không panic/watchdog/crash. Chờ đến `ELAPSED_PENDING_REVIEW` rồi kiểm tra lại các trang.
8. Đánh giá free_heap/largest_free_block hiện tại sau warm-up: không giảm theo từng vòng tương đương hoặc giảm liên tục bất thường. Một drop lúc khởi tạo/chuyển trang hoặc min water mark không đủ để chốt FAIL. Nếu có xu hướng giảm, gửi log đầy đủ; giữ PENDING cho phân tích thay vì tự kết luận ổn định.
9. Gửi kết quả từng bài và log bộ nhớ đầu/cuối (tốt nhất toàn phiên). Chỉ chuyển PASS/FAIL khi có bằng chứng phần cứng. Không tự chuyển PASS chỉ vì đủ 15 phút hoặc Build SUCCESS.

Nếu `pio` chưa có trong PATH, dùng đường dẫn PlatformIO đầy đủ như phần A.

### Test case và nghiệm thu M0.1B

| ID | Test case / kỳ vọng | Trạng thái | Bằng chứng còn thiếu |
| --- | --- | --- | --- |
| B01 | Clean/Build environment cyd_ili9341 | PASS | Log build trên máy đã có |
| B02 | Upload thành công | PASS theo người dùng | Xác nhận nghiệm thu M0.1B |
| B03 | RGB/BLACK/WHITE đúng, nền đen | PASS theo người dùng | Xác nhận trực tiếp |
| B04 | Font GLCD rõ | PASS theo người dùng | Xác nhận trực tiếp |
| B05 | Geometry/bốn góc và orientation đúng | PASS theo người dùng | Xác nhận trực tiếp |
| B06 | Touch chuyển trang | PASS theo người dùng | Xác nhận trực tiếp |
| B07 | Hiển thị System Info hoạt động | PASS theo người dùng | Xác nhận trực tiếp |
| B08 | Hoạt động ổn định 15 phút | PASS theo người dùng | Xác nhận trực tiếp |
| B09 | Phân tích riêng dãy heap/block sau warm-up | Chưa có log chi tiết | Không suy ra số liệu đo từ xác nhận ổn định |
| B10 | Màu/font/geometry | PASS theo nghiệm thu người dùng | Không có số liệu mới do agent đo |
| M0.1B | Nghiệm thu milestone trước M0.2 | PASS theo người dùng | Yêu cầu M0.2 xác nhận milestone đã PASS |

Không chuyển các kết quả B sang M0.2 tự động. Bản M0.2 có đường sự kiện touch và
NVS mới nên phải kiểm thử riêng. Không triển khai Focus Quest, Pixel Pet, Wi-Fi hoặc ứng dụng khác.

### Quay lại bản A ổn định

Khôi phục source/cấu hình đã lưu và bỏ bốn file mới của B trước Clean/Build (thực hiện thủ công nếu cần rollback):

```powershell
Copy-Item -LiteralPath 'docs/baselines/M0_FONT_TEST_01_verified_before_M0_1B_2026-11-19/src/main.cpp' -Destination 'src/main.cpp'
Copy-Item -LiteralPath 'docs/baselines/M0_FONT_TEST_01_verified_before_M0_1B_2026-11-19/platformio.ini' -Destination 'platformio.ini'
Remove-Item -LiteralPath 'include/DisplayHAL.h','include/DiagnosticScreen.h','src/DisplayHAL.cpp','src/DiagnosticScreen.cpp'
pio run -e cyd_ili9341 -t clean
pio run -e cyd_ili9341
# Chỉ Upload sau Build SUCCESS nếu muốn quay lại A:
pio run -e cyd_ili9341 -t upload --upload-port COM5
```

## M0.2 — M0_TOUCH_CAL_01 (người dùng đã nghiệm thu)

Nguồn cập nhật: yêu cầu M0.3 đính kèm xác nhận M0.0/M0.1/M0.2 hoàn thành trên
phần cứng, gồm Build/Upload, màu/GLCD, Display Diagnostics, calibration 5 điểm,
NVS lưu/khôi phục, PRESS/RELEASE/TAP và CAL Serial. Đây là xác nhận của người dùng,
không phải các phép đo board do agent tự chạy. Những ca lỗi chi tiết chưa được
báo riêng vẫn lưu PENDING trong bảng; không cản nghiệm thu tổng thể mà người dùng đã chấp nhận.

### Backup và thay đổi

Đã lưu toàn bộ source/include/config, binary/ELF/boot images, log và báo cáo của
M0.1B trước sửa tại [baselines/M0_1B_DISPLAY_DIAG_02_verified_before_M0_2_2026-11-19](baselines/M0_1B_DISPLAY_DIAG_02_verified_before_M0_2_2026-11-19/).
Có SHA256.json và BOARD_CONFIRMATION.md. Hash bản sao đã được kiểm tra lại.

- Thêm `src/hal/TouchHAL.h/.cpp`: instance XPT2046/SPI duy nhất, giữ cách begin/rotation đã dùng, poll 10 ms, debounce 40 ms, PRESS/RELEASE/TAP và khoảng chặn TAP 250 ms.
- Thêm `src/core/TouchTypes.h`, `TouchCalibration.h/.cpp`, `TouchCalibrationStore.h/.cpp`: kiểu dữ liệu chung, median 31 mẫu, affine 5 điểm, residual/3 điểm độc lập, clamp và Preferences với version/magic/CRC32.
- Thêm `src/apps/TouchCalibrationScreen.h/.cpp`, `TouchTestScreen.h/.cpp`: dấu cộng từng điểm, lấy mẫu sau settling rồi chốt khi thả, timeout/retry, năm hitbox 80×50 và TAP/HOLD.
- Sửa `src/main.cpp`: điều phối CAL/TEST/DIAG, parser Serial có giới hạn, log raw/pixel/event và heap định kỳ; save NVS chỉ khi có kết quả mới hoàn tất.
- Sửa `include/DiagnosticScreen.h`, `src/DiagnosticScreen.cpp`: thêm show() để trở về diagnostics, không re-init TFT; NEXT bằng Serial phục hồi được khi touch không phản hồi.
- Tạo [TOUCH_CALIBRATION.md](TOUCH_CALIBRATION.md) với thuật toán, ngưỡng, schema NVS, cách nạp và thao tác 5+3 điểm, kiểm thử và phục hồi.
- `platformio.ini`, `include/board_pins.h`, `include/DisplayHAL.h`, `src/DisplayHAL.cpp` giữ nguyên SHA-256 so với baseline. Không thay GPIO, bus, driver, rotation, inversion, font hoặc dependency.

TFT vẫn init → rotation 1 → text wrap false → touch begin/rotation 1 → boot log →
inversion true → load calibration hoặc bắt đầu UI. touch.begin() không phải phép
probe: thư viện đang cài trả true sau cấu hình; log mới ghi CONFIGURED và chưa
xác nhận phản hồi. Không có TFT_eSPI touch controller thứ hai.

Affine tự học swap/inversion từ raw thực, không hardcode mapping raw min/max hoặc
áp dụng flip lần hai. Chỉ ghi NVS sau residual 5 điểm đạt RMS≤8/max≤12 px và 3 điểm
độc lập đều ≤15 px. Record 88 bytes, CRC/metadata/residual được kiểm tra khi load;
sau save có readback. Thời gian chờ không chặn loop, timeout 30 giây/điểm. Không có
full-screen framebuffer/sprite hoặc malloc/new tường minh trong loop.

### Kết quả trên máy

- `pio run -e cyd_ili9341 -t clean`: **SUCCESS**, exit code 0, 0.55 giây — [log Clean](M0_TOUCH_CAL_01_clean.log).
- `pio run -e cyd_ili9341`: **SUCCESS**, exit code 0, 12.23 giây — [log Build](M0_TOUCH_CAL_01_build.log).
- RAM: **22,944 / 327,680 bytes (7.0%)**.
- Flash: **312,637 / 1,310,720 bytes (23.9%)**.
- Preferences 2.0.0 lấy từ framework Arduino hiện có; không sửa lib_deps. Cảnh báo TOUCH_CS của TFT_eSPI như các bản trước, không bật touch tích hợp.
- Kiểm tra mã/hash: cấu hình và DisplayHAL không đổi, một controller touch, một lời gọi inversion true, một vị trí putBytes chỉ sau validation. Marker phiên bản/log mới có trong BIN.
- Chưa chạy runtime affine/CRC/events/NVS trên host hoặc board trong phiên này. Không dùng build/link để đánh dấu validation hoặc touch PASS.
- Khi tạo bản M0.2 chỉ build, không tự Upload hoặc ghi NVS board. Người dùng đã nạp và nghiệm thu sau đó trước khi yêu cầu M0.3.

SHA-256 BIN M0.2 đã được sao lưu trước M0.3 (không phải BIN hiện tại):

```text
E924E31902DED52CE1EEA35458047562E4655AFAB9416619922488E34546F912
```

### Kịch bản kiểm thử M0.2

Nạp và xác nhận `FIRMWARE_VERSION: M0_TOUCH_CAL_01` theo
[hướng dẫn](TOUCH_CALIBRATION.md). Chạm dấu cộng, giữ khoảng 0,5 giây rồi nhả ở
(30,42), (290,42), (160,120), (30,208), (290,208), tiếp tục 3 dấu VERIFY độc lập.
Lưu lại raw median, số inlier, fit errors và các verify errors.

| ID | Test / kỳ vọng | Trạng thái | Bằng chứng cần thu |
| --- | --- | --- | --- |
| T01 | Clean/Build cyd_ili9341 | SUCCESS trên máy | Log đã có, không phải board PASS |
| T02 | Upload và màu/font M0.2 | PASS theo người dùng | Xác nhận trước M0.3 |
| T03 | PRESS/RELEASE/TAP | PASS theo người dùng | Xác nhận sự kiện hoạt động; giữ/kéo trên Shell vẫn phải thử riêng |
| T04 | Calibration 5 điểm | PASS theo người dùng | Xác nhận calibration 5 điểm PASS |
| T05 | Mẫu thiếu/nhiễu hoặc vị trí sai bị từ chối; không ghi NVS | PENDING | SAMPLE_REJECTED/VALIDATION_FAIL |
| T06 | Mapping được nghiệm thu | PASS theo nghiệm thu người dùng | Chưa có dãy residual số cụ thể để phân tích tại máy |
| T07 | NVS save/reload | PASS theo người dùng | NVS lưu/khôi phục PASS |
| T08 | Dấu cộng pixel gần chỗ chạm; đúng mép, clamp và 5 hitbox >=40×40 | PENDING | Touch Test, raw/pixel/hitbox |
| T09 | Khôi phục calibration khi restart | PASS theo người dùng | Xác nhận NVS khôi phục; bản Shell kiểm tra lại riêng |
| T10 | CAL qua Serial | PASS theo người dùng | CAL hoạt động; các lệnh recovery khác chưa có kết quả riêng |
| T11 | Timeout 30 giây, không treo; retry được | PENDING | TIMEOUT, retry/UI/Serial vẫn hoạt động |
| T12 | DIAG/NEXT phục hồi cả khi touch không phản hồi | PENDING | Geometry/System vẫn truy cập bằng Serial |
| T13 | NVS thiếu/hỏng/sai size/version/CRC bị từ chối | PENDING | Chạy thử với dữ liệu lỗi trên board; hiện mới kiểm tra mã |
| T14 | Không crash/reset, heap ổn định trong thao tác và 15 phút | PENDING | MEMORY/DIAG log, uptime liên tục |
| M0.2 | Nghiệm thu milestone | PASS theo người dùng | Yêu cầu M0.3 xác nhận M0.2 đã hoàn thành |

Các thông báo VALIDATION_PASS là kết quả thuật toán khi chạy, không tự thay thế
nghiệm thu người dùng. M0.3 chỉ bắt đầu sau xác nhận M0.2 nêu trên; không chuyển
nghiệm thu M0.2 thành PASS cho firmware Shell mới.

## M0.3 — DeskOS Shell v0.3 / M0_SHELL_01

### Backup và file

Backup bản M0.2 trước thay đổi:
[M0_TOUCH_CAL_01_verified_before_M0_3_2026-11-19](baselines/M0_TOUCH_CAL_01_verified_before_M0_3_2026-11-19/).
Gồm source/include, config, BIN/ELF/boot images, log, báo cáo và SHA256.json;
BOARD_CONFIRMATION.md ghi xác nhận người dùng. Toàn bộ bản sao đã được kiểm tra
hash sau build Shell. Không có thao tác đọc/xóa NVS board trong phiên này.

Thêm:

- src/core/AppTypes.h, IApp.h, AppManager.h/.cpp.
- src/ui/UITheme.h, PixelRenderer.h/.cpp.
- src/apps/HomeScreen.h/.cpp, PlaceholderScreen.h/.cpp.
- docs/ARCHITECTURE.md; log Clean/Build Shell.

Sửa main.cpp để tích hợp Shell, docs/TEST_REPORT_M0.md và ghi chú trạng thái trong
TOUCH_CALIBRATION.md/M0_0_HARDWARE_BASELINE.md.
**Tất cả module M0.2 cũ ngoài main.cpp giữ nguyên byte**, gồm DisplayHAL,
TouchHAL, calibration/types/store, CalibrationScreen/TouchTestScreen, DiagnosticScreen.
platformio.ini và board_pins.h không đổi. Thay đổi tích hợp được giải thích trong
[ARCHITECTURE.md](ARCHITECTURE.md); không sửa thuật toán/record NVS đã PASS.

### Hành vi đã triển khai

- IApp có onEnter/update(deltaMs)/render/onTouch/onExit. Registry cố định 7 ID;
  chỉ một active IApp; Home instance riêng, một PlaceholderScreen dùng chung cho 6 ID.
- Chuyển trang được xếp pending; AppManager gọi onExit cũ rồi onEnter mới và render.
  Không init lại TFT/SPI/touch, không cấp phát framebuffer hoặc app object mỗi trang.
- Home 3×2: PET, WINDOW, FOCUS, MISSIONS, PC MON, WEATHER; nền #171329 và palette
  Retro Pixel theo yêu cầu; panel viền pixel, icon tự vẽ, font GLCD 1 hiện có.
- Placeholder có đúng tên/icon theo ID, COMING SOON, BACK và HOME. Hai nút đều
  về Home vì chưa có tầng app con; BACK trên Home bị vô hiệu hóa.
- PRESS chỉ highlight. Chỉ TAP đã hiệu chuẩn và có contact hợp lệ mới mở trang.
  Giữ >700 ms hoặc di chuyển >8 pixel sẽ cancel; rời hitbox gốc cũng hủy target.
  TouchHAL debounce/cooldown M0.2 giữ nguyên, không nhận raw trực tiếp trong UI.
- Dirty regions cho card/nút nhấn-thả; full redraw chỉ khi vào trang. Loop dùng
  millis và yield, không timed delay, không có animation hoặc clock/Wi-Fi giả.
- Map hợp lệ từ NVS mở Home; thiếu/hỏng thì dùng màn hình CAL cũ. CAL thành công
  về Home. TEST vẫn mở Touch Test M0.2; DIAG/NEXT vẫn mở/chuyển Display Diagnostics.
  HOME/BACK Serial trở về Shell hoặc vào CAL nếu chưa có map, không bị mắc kẹt ở Home.
- NVS namespace/key/schema/CRC giữ nguyên. Không erase/clear và không write khi
  navigation/render; chỉ ghi ở đường hoàn thành CAL đã có trước.
- APP_SWITCH log ID/label, lifecycle+render duration_us, số chuyển và heap/largest.
  MEMORY log mỗi 5 giây; raw touch theo event hoặc khoảng 200 ms khi đang chạm.

### Kết quả trên máy

| Kiểm tra | Kết quả | Bằng chứng |
| --- | --- | --- |
| Clean cyd_ili9341 | SUCCESS | Exit code 0, 0.60 s; [log Clean](M0_SHELL_01_clean.log) |
| Build cyd_ili9341 | SUCCESS | Exit code 0, 12.08 s; [log Build](M0_SHELL_01_build.log) |
| Giữ module/config đã nghiệm thu | OK kiểm tra hash | Mọi module cũ ngoài main.cpp và config khớp backup |
| Bố cục/nhãn tĩnh | OK kiểm tra mã | Sáu card không chồng, nằm trong y=32..199, nhãn/tên vừa chiều rộng GLCD |
| Artifact | OK kiểm tra BIN | Version, sáu tên app, COMING SOON, Shell/NVS/recovery marker có trong BIN |
| Upload / kiểm thử trên board | PENDING | Không tự động Upload |

RAM **23,056 / 327,680 bytes (7.0%)**; Flash **317,317 / 1,310,720 bytes (24.2%)**.
So với M0.2, static RAM tăng 112 bytes; không có sáu framebuffer. Cảnh báo
TOUCH_CS của TFT_eSPI vẫn như trước vì touch dùng XPT2046 riêng; không đổi cấu hình
để xử lý cảnh báo. Chưa chạy runtime lifecycle/gesture bằng host compiler hoặc board
trong phiên này. Kiểm tra mã/build không thay thế nghiệm thu phần cứng.

SHA-256 `.pio/build/cyd_ili9341/firmware.bin` của Shell hiện tại:

```text
E224360C21C37314F4E0946A1BFF7CDCB3F0D2F9AC7DBB489ED1B925F0483C63
```

### Upload và cách kiểm thử

Đóng monitor trước, nạp thủ công bình thường, không chạy erase flash/NVS:

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Nếu `pio` không có trong PATH, dùng
`& 'C:\Users\Acer\.platformio\penv\Scripts\platformio.exe'`.
Nhấn EN/RESET sau khi mở monitor để thu log từ đầu. Marker kỳ vọng, không phải
log đã thu trên board Shell:

```text
FIRMWARE_VERSION: M0_SHELL_01
DISPLAY_CONFIG: INVERT_ON
SHELL_REGISTRY: READY ids=7 screen_instances=2
TOUCH_CAL: LOADED_FROM_NVS
APP_SWITCH: id=0 app=HOME duration_us=... switches=1 free_heap=... largest_free_block=...
DISPLAY_READY
```

Nếu NVS không hợp lệ thì vào CAL cũ và ghi NVS_MISSING_OR_INVALID; hoàn thành 5+3
điểm sẽ trở Home. Không yêu cầu hiệu chuẩn lại mỗi lần mở ứng dụng. Gửi DIAG để
vào recovery hoặc TEST/STATUS để đối chiếu mapping; CAL bắt đầu lại hiệu chuẩn.

Trên Home, chạm/nhả từng icon, đối chiếu placeholder đúng tên/icon và COMING SOON;
thử cả BACK và HOME. Giữ ngón >1 giây hoặc kéo >8 px rồi thả: phải ở nguyên trang,
không có APP_SWITCH mới do thao tác đó. Kiểm tra highlight trở về bình thường.

Sau warm-up, ghi switches làm mốc rồi thực hiện ít nhất 50 cặp mở placeholder→Home
(luân phiên 6 icon) để có ≥100 transition mới; so sánh chênh lệch count, không tính
boot Home ban đầu. Giữ log APP_SWITCH/MEMORY và kiểm tra không crash.

Chạy liên tục ≥30 phút trong cùng phiên (uptime_ms≥1800000), xen các vòng điều hướng
và để idle. So sánh free_heap/largest_free_block ở cùng trạng thái sau warm-up,
khoảng phút 1/5/10/20/30; không có xu hướng giảm kéo dài bất thường, không có marker
boot lặp lại hoặc uptime quay 0. Không tự suy ra leak từ một lần drop hoặc tự PASS
chỉ vì đủ thời gian. Gửi log đầu/cuối hoặc toàn phiên để đánh giá.

RESET và rút/cắm USB phải nạp lại map và mở Home; kiểm tra CAL/DIAG/HOME ở cuối
phiên. Không có test vật lý nào ở trên đã chạy trong phiên agent.

### Test case M0.3

| ID | Test / kỳ vọng | Trạng thái | Bằng chứng |
| --- | --- | --- | --- |
| T01 | Clean/Build SUCCESS | SUCCESS trên máy | Log bản cuối đã có |
| T02 | Home đủ 6 icon/label, đúng layout, font không cắt | PENDING | Màn hình thật |
| T03 | PET mở Pixel Pet placeholder đúng | PENDING | Touch + APP_SWITCH id=1 |
| T04 | WINDOW/FOCUS/MISSIONS/PC MON/WEATHER đúng | PENDING | Cả 5 ID còn lại |
| T05 | HOME và BACK về Home đúng | PENDING | Thử ở mọi placeholder |
| T06 | Hold/drag không chuyển, tap không lặp, highlight đúng | PENDING | Thao tác thật, count không tăng khi giữ/kéo |
| T07 | ≥100 transition mới không crash | PENDING | Chênh lệch switches và log cùng phiên |
| T08 | Restart vẫn giữ calibration và mở Home | PENDING | LOADED_FROM_NVS + vị trí chạm đúng sau RESET/USB |
| T09 | DIAG/NEXT/CAL/TEST/HOME phục hồi được | PENDING | Thử Serial kể cả khi không dùng touch |
| T10 | ≥30 phút không reboot, heap không giảm bất thường | PENDING | Log uptime/heap/largest theo thời gian |
| M0.3 | Nghiệm thu DeskOS Shell v0.3 | PENDING | Chờ người dùng xác nhận T02–T10 |

### Giới hạn hiện tại

Sáu ứng dụng chỉ là placeholder, không có logic app, animation phức tạp, đồng hồ,
Wi-Fi/API/NTP/PC Agent hoặc trạng thái app được lưu. BACK chỉ có một tầng về Home.
Pixel renderer chỉ hỗ trợ layout landscape/font 1 hiện tại. Chưa có runtime
lifecycle/gesture hoặc soak test M0.3; mọi giới hạn này không được che bằng Build PASS.
Kiến trúc và chi tiết ownership/input/render xem [ARCHITECTURE.md](ARCHITECTURE.md).

## Quay lại FINAL_04 ổn định

Khôi phục cả source và cấu hình trước LOAD_GLCD, rồi Clean/Build. Chỉ Upload thủ công khi muốn quay lại baseline:

```powershell
Copy-Item -LiteralPath 'docs/baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19/src/main.cpp' -Destination 'src/main.cpp'
Copy-Item -LiteralPath 'docs/baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19/platformio.ini' -Destination 'platformio.ini'
pio run -e cyd_ili9341 -t clean
pio run -e cyd_ili9341
# Chỉ upload sau Build SUCCESS:
pio run -e cyd_ili9341 -t upload --upload-port COM5
```
