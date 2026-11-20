# M0.0 — Hardware baseline và thử nghiệm color inversion

Ngày ghi nhận: 2026-11-19 (Asia/Saigon).

**Trạng thái mới nhất: M0.0/M0.1/M0.2 đã được người dùng nghiệm thu trên phần cứng, gồm display, calibration/NVS và event touch. Khóa `setRotation(1)` + `invertDisplay(true)` + LOAD_GLCD=1. Project hiện tại là M0.3 `M0_SHELL_01`, Clean/Build SUCCESS, chưa Upload; các bài thử Shell/100 chuyển trang/30 phút PENDING trong [TEST_REPORT_M0.md](TEST_REPORT_M0.md). DisplayHAL, TouchHAL, calibration và cấu hình phần cứng không đổi; xem [ARCHITECTURE.md](ARCHITECTURE.md).**

## Baseline trước thay đổi

Theo xác nhận của người dùng:

- Firmware đã Build SUCCESS và Upload SUCCESS.
- Serial Monitor hoạt động trên COM5, 115200 baud.
- ESP32 được nhận diện là ESP32-D0WD-V3.
- Cảm ứng nhận raw touch và chuyển trang 0 → 1 → 2 → 0 (người dùng xác nhận bổ sung).
- Bố cục màn hình đúng. Mô tả màu chính xác được người dùng cập nhật: đỏ → cyan, xanh lá → magenta, xanh dương → vàng; nền tối thành nền sáng. Mô tả này thay cho ghi nhận màu ban đầu.

Đã lưu bản sao trước khi chỉnh sửa tại [baselines/M0_0_pre_inversion_2026-11-19](baselines/M0_0_pre_inversion_2026-11-19/):

- `src/main.cpp`, `platformio.ini`, `include/board_pins.h`.
- Các artifact hiện có trong `.pio/build/cyd_ili9341/`: `firmware.bin`, `firmware.elf`, `bootloader.bin`, `partitions.bin`.
- `boot_app0.bin` từ framework đang cài.
- `SHA256.json` ghi đường dẫn, kích thước và SHA-256 của các file đã lưu.

Đây là các file hiện có được lưu trước lần build thử nghiệm; kết quả Upload baseline trên board dựa trên xác nhận của người dùng. Không có Git trong thư mục project hiện tại. Đã kiểm tra SHA-256 của bản sao trước và sau build thử nghiệm, không thay đổi.

SHA-256 firmware baseline:

```text
4B0563053AFF2D06F659B4F6C23F95390042C2F6BF501564A7CE21EA1E8A9D60
```

## Kiểm tra cấu hình và khởi tạo

TFT_eSPI lấy cấu hình từ `build_flags` trong `platformio.ini` qua `USER_SETUP_LOADED=1`; không sửa `User_Setup.h` trong dependency.

| Hạng mục | Cấu hình giữ nguyên |
| --- | --- |
| Environment | `cyd_ili9341` |
| Platform / board / framework | `espressif32@6.4.0` / `esp32dev` / Arduino |
| Thư viện | `bodmer/TFT_eSPI@2.5.43`; XPT2046_Touchscreen từ Git tag `v1.4` |
| TFT driver | `ILI9341_2_DRIVER=1` |
| Kích thước / rotation | 240 × 320; `tft.setRotation(1)` cho landscape 320 × 240 |
| TFT GPIO | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST -1, BL 21; backlight HIGH |
| SPI | HSPI; ghi 40 MHz, đọc 16 MHz |
| Touch GPIO | SCLK 25, MISO 39, MOSI 32, CS 33, IRQ 36; bus VSPI riêng |
| Serial / upload speed | 115200 / 460800 baud |

Trước thay đổi, `src/main.cpp` khai báo `TFT_eSPI tft;`, gọi `tft.init()` rồi `tft.setRotation(1)` và chưa gọi `invertDisplay()`.

Không khai báo `TFT_INVERSION_ON` hoặc `TFT_INVERSION_OFF` trong cấu hình project đang dùng. Dòng nhắc `-DTFT_INVERSION_ON=1` cuối `platformio.ini` chỉ là chú thích, không có hiệu lực. TFT_eSPI có xử lý hai macro này trong thư viện; sự tồn tại của mã xử lý không có nghĩa project đã bật macro.

Các lần thử dùng các hằng chuẩn `TFT_RED`, `TFT_GREEN`, `TFT_BLUE`, `TFT_BLACK`, `TFT_WHITE`. Báo cáo A/B cũ nói OFF đúng và ON sai, nhưng kết quả trực tiếp SEQUENCE_03 mới nhất cho thấy ngược lại: **ON đúng, OFF sai**. Hai bằng chứng mâu thuẫn; ưu tiên thử nghiệm mới nhất để chọn ON cho FINAL_04. Không thay RGB order hoặc gamma; chưa xác định nguyên nhân của mâu thuẫn.

## Lần 1 — thử nghiệm true (lịch sử)

Thêm đúng một dòng trong `src/main.cpp`, ngay sau khởi tạo và rotation:

```cpp
tft.init();
tft.setRotation(1);  // landscape 320x240
tft.invertDisplay(true);
tft.setTextWrap(false);
```

Không thay GPIO, driver, framework, dependency, RGB/BGR order hoặc các thông số khác. Không triển khai giao diện DeskOS mới.

## Kết quả build lần 1 — true

- Lệnh: `pio run -e cyd_ili9341` (thực thi bằng PlatformIO tại `C:\Users\Acer\.platformio\penv\Scripts\platformio.exe`).
- Kết quả: **SUCCESS**, exit code 0, PlatformIO báo 5.24 giây.
- RAM: 22,276 / 327,680 bytes (6.8%).
- Flash: 286,257 / 1,310,720 bytes (21.8%).
- Log: [M0_0_inversion_build.log](M0_0_inversion_build.log).
- Có cảnh báo `TOUCH_CS pin not defined` từ TFT_eSPI; project dùng XPT2046_Touchscreen và SPI riêng cho touch. Không thêm macro hoặc đổi cấu hình để xử lý cảnh báo trong thử nghiệm này.
- Lần chạy trong sandbox ban đầu bị chặn ghi file khóa PlatformIO ngoài workspace; lần chạy lại với quyền cần thiết đã thành công.
- Đã đối chiếu: `platformio.ini` và `include/board_pins.h` giữ nguyên byte; `main.cpp` chỉ thêm dòng `tft.invertDisplay(true);` (không tính kiểu xuống dòng).

SHA-256 firmware thử nghiệm lần 1 (nay được lưu trong bản sao `M0_0_inversion_true_2026-11-19`):

```text
60F8BE20D77205F9C1A7BE9B4A2D2D73EB705FA92A306B3AFDFF452B5042B07A
```

## Kiểm tra tiếp theo và lần 2 — false (lịch sử)

Đã kiểm tra lại source, cấu hình, log và artifact trước khi thay đổi:

- Source thực tế đã có `tft.invertDisplay(true);` ngay sau `tft.setRotation(1)`.
- `firmware.bin` có SHA-256 đúng bằng bản true đã build ở lần 1; không phải binary baseline chưa thêm inversion.
- Disassembly `setup()` trong `firmware.elf` cho thấy `setRotation(1)` rồi `movi.n a11, 1` tại `0x400d17cc`, tiếp đến lời gọi `TFT_eSPI::invertDisplay(bool)` tại `0x400d17ce`.
- Đã đối chiếu byte của hàm `setup()` trong segment ESP32 của `firmware.bin` với ELF: khớp. Vì vậy binary trên máy thực sự chứa lời gọi với tham số **true**.
- Không có macro `TFT_INVERSION_ON/OFF` được bật trong cấu hình project. Dòng ON cuối `platformio.ini` vẫn chỉ là chú thích. Trong thư viện đang dùng, true gửi `TFT_INVON` (`0x21`), false gửi `TFT_INVOFF` (`0x20`); mỗi lệnh được gửi hai lần theo cách triển khai sẵn có của TFT_eSPI.

Người dùng xác nhận Build/Upload thành công nhưng vẫn thấy các màu bổ sung và nền bị đảo sáng. Chưa đọc flash trên board hoặc đối chiếu hash của bản đang chạy, nên chưa thể xác minh độc lập rằng bản true chính là bản đã nạp. Thực hiện bản false riêng để đối chiếu theo yêu cầu; chưa kết luận chế độ nào đúng với panel này.

Trước khi đổi, đã lưu source, cấu hình, firmware BIN/ELF, log build true, `SHA256.json` và disassembly tại [baselines/M0_0_inversion_true_2026-11-19](baselines/M0_0_inversion_true_2026-11-19/). Baseline trước inversion vẫn giữ nguyên. Đã kiểm tra lại hash của cả hai bản sao sau build false.

**Thay đổi mã duy nhất trong lần 2: `true` → `false`.**

```cpp
tft.init();
tft.setRotation(1);  // landscape 320x240
tft.invertDisplay(false);
tft.setTextWrap(false);
```

Không thay driver, GPIO, RGB order, rotation, framework, dependency hoặc thành phần khác. Chưa phát triển DeskOS Core hoặc 6 ứng dụng.

Kết quả build lần 2:

- Lệnh: `pio run -e cyd_ili9341`.
- **SUCCESS**, exit code 0, PlatformIO báo 4.60 giây.
- RAM: 22,276 / 327,680 bytes (6.8%); Flash: 286,257 / 1,310,720 bytes (21.8%).
- Log riêng: [M0_0_inversion_false_build.log](M0_0_inversion_false_build.log).
- Cảnh báo `TOUCH_CS pin not defined` vẫn tồn tại như lần trước; không sửa trong lần thử này.
- Disassembly mới: [M0_0_inversion_false_setup_disassembly.txt](M0_0_inversion_false_setup_disassembly.txt), tham số tại `0x400d17cc` là `movi.n a11, 0` trước lời gọi inversion. Byte `setup()` trong BIN mới tiếp tục khớp ELF, xác nhận bản build hiện tại chứa **false**.
- Đối chiếu source với bản true: chỉ đổi tham số inversion; `platformio.ini` và `include/board_pins.h` không thay đổi.

SHA-256 bản false trước bài test A/B (đã lưu riêng, không còn là binary hiện tại):

```text
70D6175EAFE65B2EE84E56FE4FE23A964AE5E1F0B607623B9CE8A4BECBF7BFA9
```

Người dùng đã xác nhận sau khi Upload bản false: hình học đúng 320 × 240; ba ô vẫn cyan, magenta, yellow, nền tối thành sáng. Raw touch và chuyển trang hoạt động. Vì vậy chưa thể chốt cấu hình inversion từ lần thử này.

## Lần 3 — Color Inversion Diagnostic M0_COLOR_AB_01 (đã xác nhận)

Đã sao lưu firmware false trước khi sửa tại [baselines/M0_0_inversion_false_before_AB_2026-11-19](baselines/M0_0_inversion_false_before_AB_2026-11-19/): source, cấu hình, BIN/ELF, bootloader, partitions, boot_app0, log build false và báo cáo trước thay đổi. `SHA256.json` ghi hash từng file. Hash firmware khớp bản false ở trên và các bản sao đã được kiểm tra lại sau build A/B.

Hành vi trong `src/main.cpp`:

- Bắt đầu bằng `startColorState(false)`: OFF trong 5 giây đầu của bài test; tiếp theo ON trong 5 giây; tiếp tục OFF → ON lặp lại.
- Chuyển trạng thái bằng `millis()` với khoảng 5000 ms và phép trừ unsigned để xử lý tràn bộ đếm. Mốc bắt đầu được lấy sau khi vẽ và in log, cho mỗi trạng thái đã vẽ đủ ít nhất 5 giây quan sát; có thêm thời gian vẽ/chạy vòng lặp. Chỉ giữ `delay(15)` ngắn từ chương trình cũ, không dùng delay dài để định thời.
- Mỗi lần chuyển đều gọi inversion rồi vẽ lại cả trang. Năm ô từ trái sang phải là **RED, GREEN, BLUE, BLACK, WHITE**, dùng chính `TFT_RED`, `TFT_GREEN`, `TFT_BLUE`, `TFT_BLACK`, `TFT_WHITE`. Nền dùng `TFT_BLACK`, chữ và viền dùng `TFT_WHITE`.
- Trên màn hình có tên trạng thái `INVERT_OFF`/`INVERT_ON` và phiên bản `M0_COLOR_AB_01`.
- Serial in một lần `DIAGNOSTIC_VERSION: M0_COLOR_AB_01` lúc khởi động; mỗi trạng thái in đúng `COLOR_TEST: INVERT_OFF` hoặc `COLOR_TEST: INVERT_ON`.
- Chỉ có **một vị trí** gọi `tft.invertDisplay(enabled)` trong `startColorState()`. `tft.init()` và `tft.setRotation(1)` chỉ chạy một lần trong `setup()`, trước khi bắt đầu test; không có đường gọi khác ghi đè trạng thái trong vòng lặp.
- Giữ khởi tạo XPT2046 và raw touch logging. `touch.setRotation(1)` thuộc đối tượng touch riêng, không điều khiển TFT. Đã bỏ logic chuyển trang và các trang test cũ khỏi bản diagnostic; chạm màn hình không chuyển trang hoặc vẽ lại.
- Giữ nguyên `platformio.ini`, `include/board_pins.h`, ILI9341_2_DRIVER, GPIO, dependency, RGB order, gamma và cấu hình font. Chỉ dùng cỡ chữ 1/2 và font mặc định như trước, không thêm font hoặc đổi build flag.
- Chưa phát triển DeskOS Core hoặc 6 ứng dụng.

Kết quả thực tế trên máy:

- Lệnh: `pio run -e cyd_ili9341` (PlatformIO executable cài sẵn tại `C:\Users\Acer\.platformio\penv\Scripts\platformio.exe`).
- **SUCCESS**, exit code 0, PlatformIO báo 4.14 giây.
- RAM: 22,268 / 327,680 bytes (6.8%); Flash: 285,493 / 1,310,720 bytes (21.8%).
- Log: [M0_0_color_AB_01_build.log](M0_0_color_AB_01_build.log).
- Cảnh báo TFT_eSPI `TOUCH_CS pin not defined` vẫn như trước; XPT2046 dùng thư viện và bus riêng. Không sửa cấu hình trong lần thử này.
- Đã kiểm tra `firmware.bin` chứa đủ ba chuỗi log phiên bản/OFF/ON; kiểm tra source chỉ có một vị trí gọi TFT init, rotation và inversion; đối chiếu hash cấu hình và bản sao không đổi.
- Khi tạo bản A/B, chỉ build, không tự động Upload. Người dùng đã Upload và gửi kết quả xác nhận ở phần dưới.

SHA-256 diagnostic A/B đã lưu riêng trước khi tạo bản stable:

```text
AB0B5B53FBEF8A01DFC25E91CB593959A4258FAFD16CB16864A8D1FB10307062
```

Kết quả A/B cũ do người dùng xác nhận (lưu lịch sử; mâu thuẫn với SEQUENCE_03 mới nhất, không còn dùng để chọn OFF):

| Trạng thái | RED | GREEN | BLUE | Nền | Kết luận |
| --- | --- | --- | --- | --- | --- |
| INVERT_OFF | Đỏ | Xanh lá | Xanh dương | Đen | Được báo PASS ở A/B cũ; mâu thuẫn với SEQUENCE_03 |
| INVERT_ON | Cyan | Magenta | Vàng | Sáng | Màu bị đảo |

Build, Upload, Serial và raw touch hoạt động; ESP32-D0WD-V3, COM5, 115200 baud. Người dùng chưa mô tả riêng từng ô BLACK/WHITE. Kết quả A/B này cập nhật nhận định từ các lần thử trước, nhưng chưa xác nhận reset/power-cycle cho bản stable mới.

## Lần 4 — M0_DISPLAY_STABLE_01 (lịch sử, vẫn sai màu trên board)

Trước khi sửa, đã sao lưu bản A/B hoạt động tại [baselines/M0_COLOR_AB_01_verified_2026-11-19](baselines/M0_COLOR_AB_01_verified_2026-11-19/), gồm source, cấu hình, firmware BIN/ELF, bootloader, partitions, boot_app0, log A/B và báo cáo trước sửa. `SHA256.json` lưu hash; `BOARD_CONFIRMATION.md` ghi xác nhận của người dùng. Firmware A/B khớp hash trên và toàn bộ file trong manifest còn nguyên sau build stable.

Thay đổi trong `src/main.cpp`:

- Bỏ bộ định thời 5000 ms, biến trạng thái và cơ chế luân phiên OFF/ON.
- Chỉ có một lời gọi inversion trong mã ứng dụng, cố định sau init và rotation:

  ```cpp
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(false);
  ```

- Khôi phục khung `DISPLAY CHECK`, nền tím tối gốc `BG = 0x18C5` và ba ô RGB ở đúng tọa độ ban đầu. Hai ô BLACK/WHITE nằm bên dưới; ô BLACK có viền trắng để dễ phân biệt. Nền khung là tím tối như trang gốc, còn ô BLACK dùng đúng `TFT_BLACK`.
- Trang cố định, không tự chuyển hoặc đổi inversion sau 5 giây; raw touch vẫn được in nhưng không chuyển trang. Không khôi phục các trang khác trong bước chốt màu này.
- Serial in `FIRMWARE_VERSION: M0_DISPLAY_STABLE_01` và `DISPLAY_CONFIG: INVERT_OFF` lúc khởi động.
- `tft.init()` và `tft.setRotation(1)` chỉ ở `setup()`. Không có lời gọi inversion trong hàm vẽ hoặc `loop()`; không có macro inversion ON/OFF đang bật ghi đè. `touch.setRotation(1)` chỉ thuộc touch.
- Đã đối chiếu SHA-256: `platformio.ini` và `include/board_pins.h` giữ nguyên. Giữ ILI9341_2_DRIVER, GPIO, framework, dependency, RGB order, gamma và font. **Chưa thêm `-DLOAD_GLCD=1`.**

Build thực tế:

- `pio run -e cyd_ili9341`: **SUCCESS**, exit code 0, PlatformIO báo 4.51 giây.
- RAM: 22,260 / 327,680 bytes (6.8%); Flash: 285,673 / 1,310,720 bytes (21.8%).
- Log: [M0_DISPLAY_STABLE_01_build.log](M0_DISPLAY_STABLE_01_build.log).
- Cảnh báo `TOUCH_CS pin not defined` của TFT_eSPI vẫn như trước; không đổi cấu hình touch trong bước này.
- Disassembly: [M0_DISPLAY_STABLE_01_setup_disassembly.txt](M0_DISPLAY_STABLE_01_setup_disassembly.txt) cho thấy truyền tham số `0` trước `invertDisplay(bool)`, ngay sau rotation. BIN chứa đủ hai chuỗi version/config và không còn các chuỗi A/B cũ.
- Khi tạo bản stable, chỉ build và không tự động Upload. Người dùng sau đó xác nhận Build/Upload SUCCESS nhưng vẫn thấy cyan, magenta, yellow và nền sáng. Kết quả này chưa giải thích được bằng cấu hình OFF trong source; cần điều tra và kiểm chứng bản tiếp theo.

SHA-256 bản stable đã lưu trước LOCK_02:

```text
9C42F68F64A00B8650BACBAB915A808BC4B2821BAB9CE6B47ED077E1B9B05F64
```

## Lần 5 — điều tra khác biệt và M0_COLOR_LOCK_02 (lịch sử, vẫn sai màu)

Đã tìm lại source A/B thực tế tại `docs/baselines/M0_COLOR_AB_01_verified_2026-11-19/src/main.cpp`, cùng cấu hình, firmware và xác nhận board. Đã kiểm tra toàn bộ hash trong manifest A/B: còn nguyên. Không dựng lại source A/B từ trí nhớ.

So sánh trước khi sửa:

| Hạng mục | A/B M0_COLOR_AB_01 | M0_DISPLAY_STABLE_01 |
| --- | --- | --- |
| Khởi tạo TFT | `tft.init()` → `setRotation(1)` → `setTextWrap(false)` | `tft.init()` → `setRotation(1)` → `invertDisplay(false)` → `setTextWrap(false)` |
| Khi gửi OFF | Sau khởi tạo touch và log khởi động, trong `startColorState(false)` | Trước khởi tạo touch và log khởi động |
| Vẽ sau OFF | Gọi `drawColorTest()` ngay sau OFF; năm ô ngang, nền TFT_BLACK | Khởi tạo touch/log rồi mới `drawDisplayTest()`; khung nền tím tối, RGB và BLACK/WHITE thành hai hàng |
| Sau khởi động | Lặp OFF/ON mỗi 5 giây, mỗi lần gửi lệnh rồi vẽ lại | Không gửi lại inversion hoặc vẽ theo frame |
| Inversion khác | Một vị trí gọi với bool luân phiên | Một vị trí gọi false trong setup |
| Cấu hình | Không bật TFT_INVERSION_ON/OFF | Giống A/B, đã đối chiếu hash |

`TFT_INVERSION_ON=1` ở cuối `platformio.ini` chỉ là chú thích. `USER_SETUP_LOADED=1` dùng build_flags; không nạp User_Setup.h mặc định. Không thấy lời gọi inversion khác trong mã ứng dụng hoặc build flag inversion đang bật. Các hàm vẽ chỉ sử dụng lệnh vẽ, không gọi lại init/rotation/inversion. Không có bằng chứng source cho việc ghi đè OFF sau đó.

Cả A/B và stable đều vẽ bằng các hằng chuẩn trong TFT_eSPI đang cài: `TFT_RED=0xF800`, `TFT_GREEN=0x07E0`, `TFT_BLUE=0x001F`, `TFT_BLACK=0x0000`, `TFT_WHITE=0xFFFF`. Không có phép đổi sang màu bổ sung trong source. Binary stable trước Clean có hash khớp bản stable đã build.

**Nhận định:** thời điểm/thứ tự gửi OFF và cơ chế gửi lại trong A/B là khác biệt có thể kiểm thử; chưa xác nhận là nguyên nhân. Kết quả A/B được báo chưa phân biệt OFF ngay khi boot với OFF sau một lần ON. Bản LOCK_02 thử thời gian chờ theo yêu cầu, không giả định đã sửa được lỗi hoặc đã chứng minh lỗi timing. Chưa đối chiếu flash thực tế với binary trên máy.

Đã sao lưu stable trước Clean tại [baselines/M0_DISPLAY_STABLE_01_before_LOCK_02_2026-11-19](baselines/M0_DISPLAY_STABLE_01_before_LOCK_02_2026-11-19/), gồm source, cấu hình, BIN/ELF, bootloader, partitions, boot_app0, log stable, báo cáo trước sửa và SHA256.json. Đã kiểm tra lại hash cả backup stable và A/B sau build mới.

Triển khai `M0_COLOR_LOCK_02`:

- Tái sử dụng palette, tọa độ và thứ tự vẽ của A/B: nền BLACK; năm ô ngang RED/GREEN/BLUE/BLACK/WHITE; chữ và viền WHITE; chỉ thay nhãn trạng thái cố định, footer và phiên bản.
- `setup()` in version, chạy `tft.init()` và `tft.setRotation(1)` một lần; giữ `setTextWrap(false)`, ghi mốc `displayInitializedAt = millis()` rồi in `DISPLAY_INIT_COMPLETE`. Touch được khởi tạo như trước.
- `loop()` kiểm tra `!colorLockApplied && millis() - displayInitializedAt >= 1000` bằng giá trị `now` đã đọc. Không có delay 1000 ms. Thời gian thực là tối thiểu 1000 ms sau mốc hoàn tất TFT; có thể thêm thời gian một vòng lặp/khởi tạo touch.
- Đặt cờ `colorLockApplied` trước khi gọi `applyColorLock()`. Hàm này thực hiện đúng chuỗi OFF của A/B: `tft.invertDisplay(false)` → `drawColorTest()` → log `INVERT_OFF_APPLIED_AFTER_1000MS`. Log đánh dấu thao tác và vẽ lại đã xong.
- Chỉ một vị trí gọi inversion, chỉ thực hiện một lần mỗi boot nhờ cờ; không có ON, không tự re-init, không gọi inversion hoặc vẽ lại mỗi frame. Timer và cờ được khởi tạo lại khi reset. Trong giây chờ ban đầu, bảng màu mới chưa được vẽ; chỉ đánh giá màu sau log áp dụng OFF.
- Giữ raw touch logging, không cho touch chuyển trang. Chỉ giữ `delay(15)` ngắn có sẵn trong loop.
- Giữ nguyên `platformio.ini`, `include/board_pins.h`, ILI9341_2_DRIVER, GPIO, framework, dependency, RGB order, gamma và cấu hình font. **Chưa thêm LOAD_GLCD.** Không phát triển sáu ứng dụng DeskOS.

Kiểm thử thực tế trên máy:

- `pio run -e cyd_ili9341 -t clean`: **SUCCESS**, exit code 0, 0.91 giây. Log: [M0_COLOR_LOCK_02_clean.log](M0_COLOR_LOCK_02_clean.log).
- `pio run -e cyd_ili9341`: **SUCCESS**, exit code 0, 13.06 giây, biên dịch lại source/thư viện/framework sau Clean. Log: [M0_COLOR_LOCK_02_build.log](M0_COLOR_LOCK_02_build.log).
- RAM: 22,268 / 327,680 bytes (6.8%); Flash: 285,489 / 1,310,720 bytes (21.8%).
- Cảnh báo `TOUCH_CS pin not defined` của TFT_eSPI vẫn như trước; project dùng XPT2046 riêng. Không đổi cấu hình để xử lý cảnh báo trong thử nghiệm này.
- Binary chứa đủ ba chuỗi version/init/applied mới, không còn version stable cũ hoặc marker A/B ON.
- [M0_COLOR_LOCK_02_disassembly.txt](M0_COLOR_LOCK_02_disassembly.txt) ghi mã máy setup/loop/applyColorLock: truyền `0` vào inversion rồi gọi hàm vẽ; lời gọi trong loop có điều kiện thời gian và cờ một lần.
- Hash cấu hình và GPIO khớp backup A/B lẫn stable. **Chưa Upload, PENDING kết quả màn hình thật.**

Các file thay đổi/thêm trong bước này:

- Sửa `src/main.cpp` và `docs/M0_0_HARDWARE_BASELINE.md`.
- Thêm thư mục backup stable nêu trên, hai log Clean/Build và file disassembly LOCK_02.
- `.pio/build/cyd_ili9341/` được Clean và tạo lại. Không sửa file cấu hình hoặc dependency nguồn.

SHA-256 firmware LOCK_02 đã lưu trước SEQUENCE_03:

```text
70D299A050B9A854B281F2A8200AE140038A39273907DD143508F70290BDF08C
```

Người dùng xác nhận tiếp theo: LOCK_02 Build/Upload thành công nhưng vẫn cyan, magenta, yellow và nền sáng. Vì vậy chưa thể kết luận chỉ chờ 1000 ms rồi áp dụng OFF là đủ.

## Lần 6 — M0_COLOR_SEQUENCE_03 (đã có kết quả trên board)

Đã đọc lại source và cấu hình hiện tại, rồi kiểm tra chính xác backup A/B tại `docs/baselines/M0_COLOR_AB_01_verified_2026-11-19/src/main.cpp`. Source, firmware và toàn bộ file trong manifest A/B vẫn đúng SHA-256. `platformio.ini` và `include/board_pins.h` khớp cả A/B lẫn LOCK_02. Có tìm thấy mã A/B cũ; bản mới tái sử dụng logic của nó có sửa đổi theo yêu cầu, không phải khôi phục nguyên trạng firmware A/B.

So sánh A/B với LOCK_02:

| Hạng mục | A/B gốc | LOCK_02 |
| --- | --- | --- |
| Thứ tự setup | TFT init → rotation 1 → text wrap → touch init → log → OFF và vẽ | TFT init → rotation 1 → text wrap → ghi mốc thời gian → touch init → chờ tối thiểu 1000 ms → OFF và vẽ |
| Sau lần OFF đầu | ON sau khoảng 5 giây, rồi OFF, tiếp tục lặp | Không gửi ON hoặc OFF lần nữa |
| Bảng màu | Năm ô chuẩn, nền BLACK | Cùng palette, tọa độ và thứ tự vẽ; khác nhãn/phiên bản |
| Cấu hình phần cứng/font | Giữ nguyên | Khớp A/B |

Điểm chưa được kết quả cũ phân biệt là OFF ngay sau boot có giống OFF sau ON hay không. SEQUENCE_03 chỉ thử chuỗi này; chưa khẳng định nguyên nhân hoặc kết quả sửa màu.

Đã sao lưu LOCK_02 trước Clean tại [baselines/M0_COLOR_LOCK_02_before_SEQUENCE_03_2026-11-19](baselines/M0_COLOR_LOCK_02_before_SEQUENCE_03_2026-11-19/), gồm source, cấu hình, firmware BIN/ELF, bootloader, partitions, boot_app0, log Clean/Build, báo cáo và SHA256.json. Đã kiểm tra hash backup LOCK_02 và A/B sau build mới, không thay đổi.

Triển khai:

- Tái sử dụng thứ tự setup của A/B: TFT init/rotation/text wrap → touch init → log → `startColorState(OffInitial)`.
- Dùng ba trạng thái có log phân biệt:

  | Thời gian từ OFF đầu | Trạng thái | Giá trị inversion |
  | --- | --- | --- |
  | 0 đến khoảng 5000 ms | OFF_INITIAL | false (0) |
  | Khoảng 5000 đến 10000 ms | ON_TEST | true (1) |
  | Từ khoảng 10000 ms trở đi | OFF_LOCKED | false (0), giữ nguyên |

- Định thời bằng `millis()` và phép trừ unsigned; mốc 0 là thời điểm lệnh OFF đầu hoàn tất sau khởi tạo, không phải `millis() == 0` ngay khi bật nguồn. Thời điểm chuyển thực tế có sai số vòng lặp ngắn; log cung cấp timestamp và elapsed thực tế để đối chiếu.
- `startColorState()` có một vị trí gọi `tft.invertDisplay(enabled)`, sau đó vẽ lại bảng màu như A/B. Gọi đúng ba lần theo chuỗi trạng thái trong một lần boot. Hàm bỏ qua yêu cầu lặp lại cùng trạng thái. OFF_LOCKED không có chuyển tiếp, không tự ON lại, không redraw hoặc gọi inversion theo frame.
- Bảng màu giữ năm ô ngang dùng `TFT_RED`, `TFT_GREEN`, `TFT_BLUE`, `TFT_BLACK`, `TFT_WHITE`, nền TFT_BLACK, chữ/viền TFT_WHITE như A/B. Chỉ đổi phiên bản/footer và thêm tên trạng thái, vẫn dùng cỡ chữ 1/2 như trước.
- Sau mỗi lần áp dụng, in một dòng có timestamp `millis()`, `FIRMWARE_VERSION: M0_COLOR_SEQUENCE_03`, `INVERSION_APPLIED: false (0)` hoặc `true (1)`, `STATE` và `SEQUENCE_ELAPSED_MS`. Timestamp lấy ngay sau lời gọi inversion, trước redraw; dòng được gửi Serial sau redraw.
- `tft.init()` và `tft.setRotation(1)` chỉ có một vị trí trong setup, trước lần OFF đầu. Không có đường gọi init/rotation/inversion khác sau chuyển trạng thái. `touch.setRotation(1)` là đối tượng touch riêng. Không bật TFT_INVERSION_ON/OFF; dòng ON trong platformio.ini chỉ là chú thích.
- Giữ raw touch logging và trang cố định. Giữ nguyên ILI9341_2_DRIVER, toàn bộ GPIO, framework, dependency, font, RGB order và gamma. Không thêm LOAD_GLCD hoặc phát triển ứng dụng DeskOS.

Kiểm thử thực tế:

- `pio run -e cyd_ili9341 -t clean`: **SUCCESS**, exit code 0, 0.54 giây. [Log Clean](M0_COLOR_SEQUENCE_03_clean.log).
- `pio run -e cyd_ili9341`: **SUCCESS**, exit code 0, 10.75 giây. [Log Build](M0_COLOR_SEQUENCE_03_build.log).
- RAM: 22,268 / 327,680 bytes (6.8%); Flash: 285,717 / 1,310,720 bytes (21.8%).
- Cảnh báo `TOUCH_CS pin not defined` của TFT_eSPI vẫn như trước; project dùng thư viện XPT2046 riêng. Không đổi cấu hình để xử lý cảnh báo trong thử nghiệm này.
- Đã kiểm tra source: một vị trí TFT init/rotation/inversion; các lời gọi state chỉ ở setup và hai điều kiện chuyển trong loop; không có chuyển tiếp từ OFF_LOCKED.
- Binary chứa version mới, đủ ba tên trạng thái và các trường log inversion/elapsed; không còn version LOCK_02. Cấu hình và pin header giữ nguyên SHA-256 so với A/B và LOCK_02.
- **Chưa Upload; PENDING cả ba giai đoạn trên màn hình thật.**

Sửa `src/main.cpp` và báo cáo này; thêm backup LOCK_02 cùng log Clean/Build SEQUENCE_03. Artifact `.pio/build/cyd_ili9341/` được Clean và tạo lại; không sửa `platformio.ini`, pin header hoặc dependency nguồn.

SHA-256 firmware SEQUENCE_03 đã lưu trước FINAL_04:

```text
7AE09632EB685EA67B48A2ECFA44543EB04B9730552CF5E7263A1CA6E5A6761B
```

Kết quả thực tế mới nhất được người dùng xác nhận trực tiếp trên ESP32:

| Giai đoạn SEQUENCE_03 | Giá trị inversion | Kết quả màu | Nền |
| --- | --- | --- | --- |
| OFF_INITIAL, 0–5 giây | false | Sai | Trắng |
| ON_TEST, 5–10 giây | true | RGB đúng | Đen |
| OFF_LOCKED, sau 10 giây | false | Lại sai | Trắng |

**Kết quả này mâu thuẫn với báo cáo A/B cũ (OFF đúng, ON sai). Ưu tiên bằng chứng thực nghiệm mới nhất: dùng true cho FINAL_04.** Chưa xác định vì sao ghi nhận cũ ngược lại; không suy diễn rằng OFF đã ổn định hoặc chỉ cần chuỗi ON → OFF. Chưa đánh dấu Color PASS cho bản mới.

## Lần 7 — M0_COLOR_FINAL_04 (baseline ổn định, Color PASS)

Đã sao lưu SEQUENCE_03 trước Clean tại [baselines/M0_COLOR_SEQUENCE_03_before_FINAL_04_2026-11-19](baselines/M0_COLOR_SEQUENCE_03_before_FINAL_04_2026-11-19/), gồm source, cấu hình, BIN/ELF, bootloader, partitions, boot_app0, log Clean/Build, báo cáo trước sửa và SHA256.json. `BOARD_CONFIRMATION.md` ghi kết quả ba trạng thái mới nhất và mâu thuẫn với A/B cũ. Hash binary khớp SEQUENCE_03 ở trên; đã kiểm tra toàn bộ file trong manifest sau build FINAL_04, còn nguyên.

Thay đổi trong `src/main.cpp`:

- Version là `M0_COLOR_FINAL_04`; bỏ enum, biến thời gian, hàm chuyển trạng thái và mọi cơ chế chuyển inversion theo thời gian.
- Giữ nguyên thứ tự khởi tạo TFT/touch từ SEQUENCE_03: `tft.init()` → `tft.setRotation(1)` → `setTextWrap(false)` → khởi tạo touch → log khởi động.
- Sau khởi tạo và trước `drawColorTest()`, gọi đúng một lần `tft.invertDisplay(true)`. Không có `invertDisplay(false)` trong mã ứng dụng hiện tại. Không gọi init/rotation/inversion hoặc vẽ lại theo frame trong loop.
- Giữ nguyên palette, tọa độ, kích thước, vòng lặp vẽ năm ô RGB/BLACK/WHITE, nền BLACK và màu chữ/viền từ SEQUENCE_03. Đã đối chiếu toàn bộ hàm vẽ: chỉ thay nhãn inversion thành INVERT_ON, nhãn trạng thái thành FIXED, footer và giá trị phiên bản. Không thay cấu hình font.
- Serial in `FIRMWARE_VERSION: M0_COLOR_FINAL_04`, `DISPLAY_CONFIG: INVERT_ON`; `DISPLAY_READY` được in sau khi vẽ xong.
- Giữ raw touch logging và trang kiểm tra cố định. Không triển khai ứng dụng DeskOS.
- `platformio.ini` và `include/board_pins.h` không thay đổi SHA-256. Giữ ILI9341_2_DRIVER, tất cả GPIO, rotation 1, framework, dependency, RGB order, font và gamma. **Chưa thêm LOAD_GLCD.**
- Không có build flag TFT_INVERSION_ON hoặc TFT_INVERSION_OFF đang bật; dòng gợi ý ON cuối platformio.ini chỉ là chú thích. Không thêm macro inversion để tránh cấu hình trùng với lời gọi runtime duy nhất.

Kiểm thử thực tế trên máy:

- `pio run -e cyd_ili9341 -t clean`: **SUCCESS**, exit code 0, 0.62 giây. [Log Clean](M0_COLOR_FINAL_04_clean.log).
- `pio run -e cyd_ili9341`: **SUCCESS**, exit code 0, 11.34 giây. [Log Build](M0_COLOR_FINAL_04_build.log).
- RAM: 22,260 / 327,680 bytes (6.8%); Flash: 285,421 / 1,310,720 bytes (21.8%).
- Cảnh báo TFT_eSPI `TOUCH_CS pin not defined` vẫn như trước; project dùng XPT2046 riêng. Không đổi cấu hình trong bước này.
- Binary chứa version FINAL_04 và hai marker DISPLAY_CONFIG/READY; không còn version SEQUENCE_03 hoặc các tên trạng thái OFF_INITIAL/OFF_LOCKED.
- Kiểm tra source: chỉ một lời gọi inversion, tham số true; không có lời gọi false. Chỉ setup khởi tạo TFT/rotation và vẽ một lần.
- Khi tạo FINAL_04, chỉ build và để Color PENDING. Người dùng đã nạp sau đó và xác nhận vượt qua toàn bộ kiểm tra màu trước khi yêu cầu M0.1; cập nhật Color PASS theo xác nhận đó.

Sửa `src/main.cpp` và báo cáo này; thêm backup SEQUENCE_03, log Clean/Build FINAL_04. `.pio/build/cyd_ili9341/` được Clean và tạo lại. Không sửa platformio.ini, pin header hoặc dependency nguồn.

SHA-256 firmware FINAL_04 đã được sao lưu trước khi build M0.1:

```text
3FD106A676E5F1CA0DC4348EBDA548BCD892FD7F650A5489E940D58F0EC9E299
```

## Upload và quan sát trên board thật

Phần này lưu quy trình kiểm tra FINAL_04 đã hoàn thành. Project hiện tại là M0.1B; để nạp bản diagnostics mới, xem [TEST_REPORT_M0.md](TEST_REPORT_M0.md). Nếu muốn quay lại FINAL_04, trước hết khôi phục cả `src/main.cpp` và `platformio.ini` từ `docs/baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19/`, rồi build lại trước khi dùng các lệnh dưới đây.

1. Đóng Serial Monitor để giải phóng COM5 (CLI: `Ctrl+C`).
2. Trong PlatformIO Terminal tại thư mục project, chạy:

   ```powershell
   pio run -e cyd_ili9341 -t upload --upload-port COM5
   ```

3. Sau Upload SUCCESS, mở lại monitor:

   ```powershell
   pio device monitor -p COM5 -b 115200
   ```

4. Xác nhận đúng ba dòng dưới đây trên Serial, có thể xen log touch init/heap. Nếu monitor mở sau lúc boot và bỏ lỡ log, nhấn EN/RESET để xem từ đầu. Đây là log kỳ vọng từ source, chưa phải log thu trên board của FINAL_04.

   ```text
   FIRMWARE_VERSION: M0_COLOR_FINAL_04
   DISPLAY_CONFIG: INVERT_ON
   DISPLAY_READY
   ```

5. Ngay sau boot/`DISPLAY_READY`, kiểm tra RGB đúng, BLACK đen, WHITE trắng và nền đen. Giữ chạy ít nhất 30 giây và kiểm tra lại, không tự đổi màu.
6. Nhấn RESET ba lần riêng biệt, đợi mỗi lần vẽ xong rồi kiểm tra màu; ghi kết quả RESET 1, 2, 3.
7. Rút USB để ngắt nguồn rồi cắm lại; nếu có nguồn khác đang cấp thì ngắt nguồn đó để board thật sự mất nguồn. Kiểm tra lại màu sau boot, mở lại COM5 nếu monitor mất kết nối.
8. Gửi kết quả từng dòng bên dưới. **Chỉ đánh dấu Color PASS sau khi tất cả mục đều hoàn thành và đúng màu**, kể cả ba lần RESET. Nếu thiếu hoặc sai bất kỳ mục nào, giữ Color PENDING và ghi lỗi tương ứng. Không thêm LOAD_GLCD hoặc triển khai ứng dụng DeskOS trong bước này.

| Kiểm tra FINAL_04 | RGB đúng | BLACK/WHITE đúng | Nền đen | Trạng thái |
| --- | --- | --- | --- | --- |
| Ngay sau boot | Đúng | Đúng | Đúng | PASS theo người dùng |
| Sau 30 giây | Đúng | Đúng | Đúng | PASS theo người dùng |
| RESET 1 | Đúng | Đúng | Đúng | PASS theo người dùng |
| RESET 2 | Đúng | Đúng | Đúng | PASS theo người dùng |
| RESET 3 | Đúng | Đúng | Đúng | PASS theo người dùng |
| Rút/cắm USB | Đúng | Đúng | Đúng | PASS theo người dùng |

Nguồn xác nhận: yêu cầu M0.1 của người dùng, ghi rõ FINAL_04 đã vượt qua toàn bộ kiểm tra màu và RGB/nền đen đúng sau reset/cấp nguồn lại. Đây là kết quả trên board do người dùng cung cấp, không phải tự suy ra từ build.

Nếu `pio` chưa có trong PATH, thay `pio` bằng `& 'C:\Users\Acer\.platformio\penv\Scripts\platformio.exe'` trong các lệnh PowerShell trên.

## Quay lại baseline

Đóng Serial Monitor. Tại thư mục project, khôi phục mã khởi tạo trước thử nghiệm rồi build và upload lại:

```powershell
Copy-Item -LiteralPath 'docs/baselines/M0_0_pre_inversion_2026-11-19/src/main.cpp' -Destination 'src/main.cpp'
pio run -e cyd_ili9341
# Chỉ upload sau khi build SUCCESS:
pio run -e cyd_ili9341 -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Với các bản M0.0 cũ, khi rollback từ M0.1 phải khôi phục cả `src/main.cpp` và `platformio.ini` tương ứng để lấy lại cấu hình trước LOAD_GLCD; `include/board_pins.h` vẫn giữ nguyên. Các binary baseline được giữ trong thư mục bản sao, không bị lần build mới ghi đè.

Để quay lại bản true thay vì baseline ban đầu, khôi phục `src/main.cpp` từ `docs/baselines/M0_0_inversion_true_2026-11-19/src/main.cpp`, rồi thực hiện build và upload như trên.

Để quay lại bản false ngay trước bài A/B, khôi phục `src/main.cpp` từ `docs/baselines/M0_0_inversion_false_before_AB_2026-11-19/src/main.cpp`, rồi build và upload như trên.

Để quay lại bản A/B đã được xác nhận, khôi phục `src/main.cpp` từ `docs/baselines/M0_COLOR_AB_01_verified_2026-11-19/src/main.cpp`, rồi build và upload như trên.

Để quay lại bản stable trước LOCK_02, khôi phục `src/main.cpp` từ `docs/baselines/M0_DISPLAY_STABLE_01_before_LOCK_02_2026-11-19/src/main.cpp`, rồi build và upload như trên. Bản này đã được người dùng báo vẫn sai màu.

Để quay lại LOCK_02 trước SEQUENCE_03, khôi phục `src/main.cpp` từ `docs/baselines/M0_COLOR_LOCK_02_before_SEQUENCE_03_2026-11-19/src/main.cpp`, rồi build và upload như trên. LOCK_02 cũng đã được người dùng báo vẫn sai màu.

Để quay lại SEQUENCE_03 trước FINAL_04, khôi phục `src/main.cpp` từ `docs/baselines/M0_COLOR_SEQUENCE_03_before_FINAL_04_2026-11-19/src/main.cpp`, rồi build và upload như trên. Đây là bản ba trạng thái đã cho bằng chứng mới nhất: true đúng, false sai.

## Tổng hợp bằng chứng và bước tiếp theo

- Báo cáo A/B cũ nói OFF đúng, ON sai; **mâu thuẫn với SEQUENCE_03 mới nhất**, không còn dùng để chọn OFF.
- Build, Upload, Serial, raw touch trên bản A/B hoạt động; các lần đầu đã kiểm tra chuyển trang 0 → 1 → 2 → 0.
- Bản `M0_DISPLAY_STABLE_01`: người dùng xác nhận **Build/Upload SUCCESS nhưng vẫn CMY, nền sáng**.
- Bản `M0_COLOR_LOCK_02`: người dùng xác nhận **Build/Upload SUCCESS nhưng vẫn CMY, nền sáng**.
- Bản `M0_COLOR_SEQUENCE_03`: người dùng xác nhận trực tiếp **false sai/nền trắng → true RGB đúng/nền đen → false lại sai/nền trắng**. Ưu tiên bằng chứng này, chọn INVERT_ON.
- Bản `M0_COLOR_FINAL_04`: **Color PASS theo xác nhận toàn bộ kiểm tra trên board của người dùng**. Giữ rotation 1, inversion true, ILI9341_2_DRIVER và GPIO/dependency hiện có.
- Đã lưu baseline xác nhận tại `docs/baselines/M0_COLOR_FINAL_04_verified_before_M0_1_2026-11-19/`, gồm source, platformio.ini trước LOAD_GLCD, pin header, BIN/ELF và các boot image, log, SHA256.json, BOARD_CONFIRMATION.md. Đã đối chiếu hash bản sao sau build font.
- `-DLOAD_GLCD=1`: đã thêm theo yêu cầu M0.1 giai đoạn A sau khi người dùng xác nhận Color PASS. Không đổi build flag cũ.
- Bản `M0_FONT_TEST_01`: người dùng đã nạp, xác nhận font GLCD/RGB/nền đen đúng sau RESET và chấp nhận giai đoạn A.
- Đã sao lưu bản A trước M0.1B tại `docs/baselines/M0_FONT_TEST_01_verified_before_M0_1B_2026-11-19/`.
- M0.1B DisplayHAL/DiagnosticScreen đã triển khai, Clean/Build SUCCESS; chưa Upload. Các kiểm tra board và 15 phút còn PENDING, xem [TEST_REPORT_M0.md](TEST_REPORT_M0.md).
