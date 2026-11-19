# CYD DeskOS M0.2 — Touch Calibration

Firmware: **M0_TOUCH_CAL_01**. Ngày: 2026-11-19 (Asia/Saigon).

**M0.2 đã được người dùng nghiệm thu trên board trước yêu cầu M0.3: calibration 5 điểm, NVS save/reload, PRESS/RELEASE/TAP và CAL Serial đã PASS theo xác nhận đó. Tài liệu này giữ thuật toán/schema/quy trình M0.2. Project hiện tại là M0.3 M0_SHELL_01, chưa Upload và chưa nghiệm thu Shell.**

Tích hợp M0.3 giữ nguyên mọi source calibration/TouchHAL và record NVS. Với map
hợp lệ, boot và hoàn thành CAL sẽ mở Home thay vì tự mở Touch Test. Gửi `TEST`
để vào màn hình kiểm tra ở tài liệu này, `HOME` để trở Shell. Marker firmware mới
là `M0_SHELL_01`; các ví dụ marker M0_TOUCH_CAL_01 bên dưới là lịch sử M0.2.
Xem [ARCHITECTURE.md](ARCHITECTURE.md) và [TEST_REPORT_M0.md](TEST_REPORT_M0.md)
cho hướng dẫn nạp bản hiện tại. Không thêm phép đảo trục hoặc đổi schema khi tích hợp.

## Cấu hình cố định và backup

M0.1B đã được người dùng nghiệm thu. Giữ nguyên environment cyd_ili9341,
ILI9341_2_DRIVER, GPIO và SPI, landscape 320×240, TFT rotation 1,
invertDisplay(true), LOAD_GLCD=1, TFT_eSPI 2.5.43 và XPT2046 tag v1.4.
Không đổi framework/dependency. Preferences là thư viện có sẵn trong Arduino ESP32,
không thêm lib_deps hoặc package.

Backup trước sửa:
[M0_1B_DISPLAY_DIAG_02_verified_before_M0_2_2026-11-19](baselines/M0_1B_DISPLAY_DIAG_02_verified_before_M0_2_2026-11-19/).
Gồm toàn bộ src/include, platformio.ini, firmware BIN/ELF, bootloader/partitions/boot_app0,
log, báo cáo cũ, SHA256.json và BOARD_CONFIRMATION.md. Đã đối chiếu hash bản sao.

## Upload và lần chạy đầu

Đóng Serial Monitor để giải phóng COM5. Tại thư mục project, dùng PlatformIO Terminal:

```powershell
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Nếu `pio` không có trong PATH, thay bằng
`& 'C:\Users\Acer\.platformio\penv\Scripts\platformio.exe'`.
Nhấn EN/RESET sau khi mở monitor nếu cần thu log từ đầu.

Các marker kỳ vọng từ mã (chưa phải log thu trên board):

```text
FIRMWARE_VERSION: M0_TOUCH_CAL_01
TOUCH_INIT: CONFIGURED (begin is not a hardware response test)
DISPLAY_CONFIG: INVERT_ON
TOUCH_CAL: NVS_MISSING_OR_INVALID
TOUCH_CAL: START
TOUCH_CAL: POINT 1/5
DISPLAY_READY
```

`touch.begin()` trong thư viện hiện có chỉ cấu hình SPI/CS/IRQ và trả true.
CONFIGURED không phải bằng chứng thiết bị đã phản hồi. FIRST_VALID_RAW chỉ cho
biết đã nhận một bộ raw vượt bộ lọc điện học; nghiệm thu vẫn cần thao tác thật.

## Thao tác hiệu chuẩn

Mỗi lần chỉ có **một dấu cộng vàng**. Chạm đúng tâm dấu cộng, giữ nhẹ khoảng
0,5 giây cho đủ mẫu, rồi **thả hoàn toàn**. Không kéo tay sang điểm kế tiếp.
Chỉ sau RELEASE và cửa sổ mẫu ổn định mới chốt điểm.

| Thứ tự | Điểm | Pixel đích |
| --- | --- | --- |
| 1 | Top Left | (30,42) |
| 2 | Top Right | (290,42) |
| 3 | Center | (160,120) |
| 4 | Bottom Left | (30,208) |
| 5 | Bottom Right | (290,208) |

Log chốt điểm, với giá trị đo thật thay cho dấu ba chấm:

```text
TOUCH_CAL: RAW x=... y=... z=... samples=... inliers=...
TOUCH_CAL: POINT_ACCEPTED
```

Sau điểm 5, firmware tính affine và kiểm tra residual. Nếu đạt, tiếp tục
**3 điểm kiểm tra độc lập**: (80,80), (240,160), (160,60). Màn hình/log ghi VERIFY 1/3…3/3.
Vẫn chạm, giữ, thả từng dấu cộng; đây là kiểm tra sai số ngoài 5 điểm đã dùng để fit.
Chưa ghi NVS ở bước fit 5 điểm.

Chỉ khi các điểm độc lập cũng đạt mới có:

```text
TOUCH_CAL: VALIDATION_PASS
TOUCH_CAL: SAVED_TO_NVS
TOUCH_TEST: START zones=5 size=80x50
```

Nếu SAVE_FAIL, firmware không tự chuyển sang dùng candidate mới; có thể RETRY/CAL,
DIAG, hoặc TEST để dùng bản calibration hợp lệ đã có trong RAM (nếu có).

Mỗi điểm có hạn 30 giây. Thiếu mẫu/nhiễu sẽ có SAMPLE_REJECTED và giữ nguyên điểm;
timeout hoặc validation fail sẽ dừng quy trình và không ghi candidate vào NVS.
Màn hình lỗi có ô RETRY, nhưng do chưa có mapping tin cậy, **chạm nhanh bất kỳ**
đều có thể thử lại sau khi đã thả tay. Serial RETRY/CAL luôn là đường thử lại độc lập.
Thử lại bắt đầu từ điểm 1.

## Thuật toán và ngưỡng ban đầu

- TouchHAL là chủ sở hữu duy nhất của SPIClass(VSPI) và XPT2046_Touchscreen.
  Giữ nguyên spi.begin với các PIN_TOUCH_* và controller.setRotation(1).
  Không dùng TFT_eSPI touch hoặc setTouch/calibrateTouch.
- Poll theo millis mỗi tối thiểu 10 ms, thư viện hiện tại tự lọc các lần đọc quá sát
  3 ms. Dùng touched()/getPoint() như bản trước.
- PRESS/RELEASE đòi trạng thái raw ổn định 40 ms. TAP phát một lần khi RELEASE,
  với thời gian giữ đã debounce ≤700 ms và dịch chuyển cực đại theo từng trục
  ≤160 đơn vị raw từ PRESS; chặn hai TAP gần nhau dưới 250 ms.
  Giữ lâu hoặc kéo không tạo TAP. PRESS và RELEASE vẫn được báo.
- Điểm raw hợp lệ có 0<x,y<4095 và 400≤z≤8190. Đây là giới hạn điện học của
  ADC/pressure, **không phải raw min/max dùng để ánh xạ**. Không suy ra calibration
  từ chúng. Các giá trị min/max mapping không được hardcode.
- Chỉ thu mẫu sau 80 ms tính từ PRESS đã debounce. Cửa sổ vòng cố định tối đa
  31 mẫu, yêu cầu ít nhất 21 mẫu. Median X/Y đầu lọc các mẫu cách median quá 90
  raw units ở một trong hai trục. Phải còn ≥21 inlier và ≥80% số mẫu; span inlier
  mỗi trục ≤120. Dùng median X/Y/Z của các inlier làm điểm đại diện.
- Fit least squares affine 2D từ cả 5 cặp raw→pixel:

  ```text
  pixelX = a*rawX + b*rawY + c
  pixelY = d*rawX + e*rawY + f
  ```

  Raw được chuẩn hóa/center trong lúc giải để giảm sai số số học. Kiểm tra span
  quan sát ≥200 raw units/trục, ma trận không suy biến và hệ số hữu hạn.
  Cross terms và dấu hệ số tự biểu diễn hoán đổi/đảo/xiên trục từ dữ liệu thực;
  không thêm flip/swap hoặc đảo trục lần thứ hai. Touch rotation vẫn là 1.
- Validation 5 điểm: RMS ≤8 px, sai số Euclidean lớn nhất ≤12 px. Kiểm tra độc lập
  3 điểm: sai số lớn nhất ≤15 px. Mỗi sai số độc lập được in Serial.
  Sai số được tính **trước clamp**; không che mẫu sai bằng ép vào mép màn hình.
- Khi dùng mapping hợp lệ, làm tròn và clamp tọa độ ra x=0..319, y=0..239.
  Đây là phép extrapolate affine tới mép từ các đích nội vi; cần kiểm tra mép thật.

Các ngưỡng trên là tiêu chí validation trong firmware, chưa phải độ chính xác đo
được trên board. Nếu chưa đạt thì giữ PENDING, xem raw/error rồi mới cân nhắc thay
ngưỡng; không tự giảm tiêu chí để đánh dấu PASS.

## Preferences / NVS

- Namespace `cyd-touch`, key `affine`, một blob 88 bytes.
- Magic 0x5443414C, version 1, bytes, width=320, height=240, rotation=1,
  verificationCount=3, reserved, 6 hệ số float, residual, raw của 5 điểm và 3 điểm
  độc lập, independent max error, CRC32 polynomial 0xEDB88320.
- Load kiểm tra đúng độ dài trước khi đọc; kiểm tra CRC, magic/version/dimensions/
  rotation/reserved, hệ số hữu hạn/không suy biến, raw và tính lại residual của cả
  hai nhóm. Sai/bị thiếu sẽ vào hiệu chuẩn, không sử dụng dữ liệu hỏng.
- Save chỉ được gọi một lần khi screen có result đã hoàn tất cả hai validation.
  Store kiểm tra lại record trước putBytes, sau ghi đọc lại, validate và memcmp.
  Không ghi NVS khi poll, draw, log, test touch hoặc đổi trang.
- CAL không xóa record cũ khi bắt đầu. Trong lần hiệu chuẩn lại, bản cũ trong RAM
  vẫn có thể dùng qua TEST; candidate chỉ thay thế sau save/readback thành công.
  Nếu reset trước khi lưu, record cũ hợp lệ vẫn được nạp. Độ bền lưu sau reset/
  mất nguồn và các ca hỏng dữ liệu vẫn cần kiểm thử trên board.
- Restart có record hợp lệ: in LOADED_FROM_NVS, hệ số/errors và vào Touch Test.

## Touch Test và phục hồi

Năm hitbox độc lập 80×50 px (tối thiểu yêu cầu 40×40):

| Zone | Góc trái trên | Kích thước |
| --- | --- | --- |
| 1 | (10,56) | 80×50 |
| 2 | (230,56) | 80×50 |
| 3 | (120,116) | 80×50 |
| 4 | (10,176) | 80×50 |
| 5 | (230,176) | 80×50 |

Màn hình hiển thị raw, pixel, Z, event và tap count; dấu cộng đi theo điểm đã map.
Chỉ TAP tăng count/hitbox; giữ >700 ms hiển thị HOLD và không tính TAP khi thả.
Zone=0 trong log nghĩa là ngoài các hitbox, không phải ngoài màn hình.
Nhả trước khi bắt đầu tương tác ở view mới để tránh chạm cũ được dùng hai lần.
Kiểm tra thêm các điểm không nằm trong calibration/verification và gần mép.

Serial nhận lệnh không phân biệt hoa/thường, kết thúc bằng Enter/CR/LF:

| Lệnh | Hành vi |
| --- | --- |
| CAL | Bắt đầu lại 5 điểm, kể cả khi mapping hiện có sai |
| RETRY | Như CAL; dùng sau timeout, nhiễu hoặc validation fail |
| TEST | Mở Touch Test với calibration đang có; thiếu map thì vào CAL |
| DIAG | Mở Display Diagnostics M0.1B, không cần mapping |
| NEXT | Vào DIAG hoặc chuyển bài test của DIAG bằng Serial |
| STATUS | In hệ số và sai số của mapping đang có, không ghi NVS |
| HELP | In danh sách lệnh |

Trong DIAG, TAP mới cũng chuyển COLORS→FONT→GEOMETRY→SYSTEM; CAL/TEST quay lại
M0.2. Không có re-init TFT hoặc đổi inversion khi chuyển view.

Log sự kiện/tọa độ có giới hạn: PRESS/RELEASE/TAP theo cạnh, raw/pixel/HOLD tối đa
khoảng mỗi 200 ms khi đang chạm; điểm calibration chỉ log median sau thả.
MEMORY in free heap/largest block/uptime/view mỗi 5 giây. Parser Serial giới hạn
24 byte/lệnh, xử lý tối đa 32 byte/loop. Buffer mẫu/ký tự cố định, không có full-screen
framebuffer/sprite hay malloc/new tường minh trong loop. Delay 1 ms chỉ để nhường
scheduler, mọi deadline dùng millis; timeout không chặn Serial hoặc diagnostics.

## Mã nguồn và kiểm thử

```text
src/hal/TouchHAL.h/.cpp                    controller duy nhất, polling/events
src/core/TouchTypes.h                     raw/pixel/frame chung
src/core/TouchCalibration.h/.cpp          median, affine, residual, CRC, clamp
src/core/TouchCalibrationStore.h/.cpp     Preferences load/save/readback
src/apps/TouchCalibrationScreen.h/.cpp    5+3 điểm, release/timeout/retry
src/apps/TouchTestScreen.h/.cpp           dấu cộng, 5 hitbox, TAP/HOLD
src/main.cpp                             điều phối view/Serial/log/NVS
include/DiagnosticScreen.h                thêm show() để phục hồi
src/DiagnosticScreen.cpp                  vẽ lại khi quay về DIAG
```

DisplayHAL.h/.cpp, board_pins.h, platformio.ini không đổi hash so với backup.
Không có module touch trùng; đã đọc source XPT2046 và Preferences đang cài trước
khi refactor. Không triển khai M0.3, ứng dụng DeskOS hoặc mạng.

Build cuối thực tế: Clean SUCCESS (0.55 s), Build SUCCESS (12.23 s), exit code 0.
RAM 22,944 / 327,680 bytes (7.0%); Flash 312,637 / 1,310,720 bytes (23.9%).
Log: [Clean](M0_TOUCH_CAL_01_clean.log), [Build](M0_TOUCH_CAL_01_build.log).
Cảnh báo TOUCH_CS của TFT_eSPI giữ như trước: touch dùng XPT2046 riêng.

SHA-256 BIN hiện tại:

```text
E924E31902DED52CE1EEA35458047562E4655AFAB9416619922488E34546F912
```

Chưa chạy runtime affine/CRC/event/NVS bằng host compiler hoặc trên ESP32 trong
phiên này; xác nhận hiện có là build/link, kiểm tra mã và hash. Các ca runtime
trong [TEST_REPORT_M0.md](TEST_REPORT_M0.md) đều PENDING tới khi người dùng xác nhận.
Build SUCCESS không phải nghiệm thu M0.2.
