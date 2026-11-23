# Known Issues — DESKOS_1.0.0-RC1

Cập nhật 2026-11-20. Phân loại: **HW-PENDING** (chưa có bằng chứng phần cứng),
**LIMITATION** (giới hạn đã biết, chấp nhận cho v1.0), **DESIGN** (hành vi có chủ đích),
**DOC** (vấn đề tài liệu). Hiện **không có FAIL mở** từ kiểm thử phần mềm.

## Phần cứng / runtime

Pixel Pet VOLT (2026-11-23) đã build và qua toàn bộ test trên máy. Sprite 48×48 phóng 2×,
11 màu, buffer ghép 2.352 B RAM tĩnh, tương tác chạm (vuốt/giữ được nhận qua `Cancel` của
`AppManager`: vuốt > 8 px, giữ > 700 ms). VOLT chưa nạp. Màu thật của panel, độ mượt, độ nhạy
vuốt/giữ trên cảm ứng điện trở vẫn **HARDWARE PENDING**. Ảnh xem trước chỉ chứa sprite, không
mô phỏng effect ở lề hoặc phần cứng. Xem [báo cáo](PET_VOLT.md).

| # | Loại | Vấn đề | Ảnh hưởng | Cách theo dõi / hướng xử lý |
| --- | --- | --- | --- | --- |
| K1 | HW-PENDING | M2–M6 và RC1 chưa hoàn tất nghiệm thu trên board; các bản prototype pet đã có log nạp/boot/network ngày 2026-11-21 | Chưa thể phát hành v1.0.0 | [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md) |
| K2 | HW-PENDING | Heap runtime khi Wi-Fi + TLS chưa đo (TLS cần ~40 KB liên tục) | Nếu phân mảnh, HTTPS có thể lỗi `CONNECT_FAILED` | Log `MEMORY … largest_free_block / min_free_heap`, `HTTPS … max_block`; logger báo nếu < 45 KB |
| K3 | HW-PENDING | Wi-Fi tăng dòng; nguồn USB yếu có thể gây brownout | Reset bất ngờ | Logger đếm `Brownout`/`rst:`; dùng cổng/cáp tốt |
| K4 | LIMITATION | Framework bật `TASK_WDT_PANIC` (5 s, theo dõi IDLE0). HTTPS task đã hạ xuống priority 0 để không bỏ đói IDLE0 | Rủi ro còn lại thấp; chưa đo thời gian handshake thật | Log `HTTPS: … ms=`; tìm `Task watchdog got triggered` |
| K5 | LIMITATION | `WiFiUDP::parsePacket()` của framework cấp phát buffer tạm cho mỗi gói (agent gửi 2 s/lần) | Không rò rỉ, nhưng có thể góp phần phân mảnh | Theo dõi S03 trong stress test |

## Bảo mật

| # | Loại | Vấn đề | Ảnh hưởng | Hướng xử lý tương lai |
| --- | --- | --- | --- | --- |
| K6 | LIMITATION | mbedTLS của Arduino-ESP32 2.0.11 tắt `MBEDTLS_HAVE_TIME_DATE`. RC1 tự kiểm `notBefore/notAfter` của **chứng chỉ leaf** sau handshake, **chỉ khi đã có NTP** (`cert_date=VALID`); khi chưa có NTP ghi `DATE_UNCHECKED_NO_NTP`. Ngày của chứng chỉ trung gian không được kiểm (chữ ký và hostname vẫn được kiểm đầy đủ) | Một leaf hết hạn bị từ chối khi có giờ; trước khi NTP sync (tối đa ~20 s đầu, WeatherService chờ NTP) vẫn chấp nhận | Khi nâng framework có `MBEDTLS_HAVE_TIME_DATE`, có thể kiểm cả chain trong handshake. **Không** dùng `setInsecure()` |
| K7 | LIMITATION | CA bundle cố định 4 root (ISRG X1/X2, GTS R1, DigiCert G2). ISRG Root X1 hết hạn 2035-06-04 | Nếu Open-Meteo đổi CA ngoài 4 root này → `CONNECT_FAILED`, Weather dùng cache | Cập nhật `src/net/RootCertificates.h` |
| K8 | LIMITATION | PC Monitor xác thực (HMAC-SHA256) nhưng **không mã hóa** | Người cùng LAN đọc được CPU/RAM/DISK/tên máy | Chấp nhận cho LAN gia đình |
| K9 | LIMITATION | Replay sau reboot: RC1 lưu "counter floor" vào NVS (lần đầu mỗi boot, sau đó ≤ 1 lần/15 phút, chỉ khi có NTP). Còn lại: trước khi NTP sync sau reboot, gói bắt được trong ≤ 15 phút trước reboot có thể được nhận một lần (chỉ hiện số liệu cũ). Không có NTP thì floor không được lưu (như M6) | Rất thấp: không có lệnh nào trong giao thức | — |
| K10 | LIMITATION | Nếu đồng hồ PC bị lùi (> vài phút) rồi khởi động lại agent, counter mới nhỏ hơn floor → `last_reject=REPLAY` cho tới khi đồng hồ vượt lại | PC Monitor tạm không kết nối | Sửa giờ Windows; hoặc `PC TOKEN NEW` (xóa floor theo token) |
| K11 | LIMITATION | Token in qua Serial (cần truy cập vật lý) và lưu dạng JSON trong `%APPDATA%\DeskOS\agent.json` | Ai đọc được profile Windows có token | Logger `hw_validation` tự che token; không commit log thô |
| K12 | LIMITATION | Mật khẩu Wi-Fi lưu trong NVS không mã hóa (không bật NVS encryption) | Người có board + công cụ đọc flash có thể lấy | Ngoài phạm vi v1.0 (cần flash encryption) |

## Hành vi có chủ đích

| # | Loại | Vấn đề |
| --- | --- | --- |
| K13 | DESIGN | Khi có NTP, Daily Missions tự sang ngày lúc 00:00 (UTC+7). NEW DAY thủ công vẫn dùng được bất kỳ lúc nào, nên người dùng có thể mở thêm "ngày" trong cùng ngày lịch (mỗi ngày quest có reward key riêng, không cộng trùng trong một ngày quest) |
| K14 | DESIGN | Múi giờ cố định Asia/Ho_Chi_Minh (UTC+7, không DST) |
| K15 | DESIGN | Pet lưu số lần vuốt khi rời app; mất nguồn khi đang ở màn Pet mất các lần vuốt của phiên đó (XP không ảnh hưởng) |
| K16 | DESIGN | Không có `psutil` → PC Monitor hiện `NET N/A` (CPU/RAM/DISK/pin vẫn có) |
| K17 | DESIGN | Chỉ hỗ trợ panel ILI9341; bản CYD ST7789 cần đổi driver và nghiệm thu lại M0 |
| K18 | DESIGN | Không có OTA; chỉ nạp qua USB. Partition mặc định (app 1,310,720 B, đang dùng 79.2%) |

## Kiểm thử / tài liệu

| # | Loại | Vấn đề |
| --- | --- | --- |
| K19 | LIMITATION | Test UI dùng draw spy (lệnh vẽ, biên 320×240, độ rộng chữ GLCD), không mô phỏng điểm ảnh, SPI hay cảm ứng thật |
| K20 | DOC | `TEST_REPORT_M1.md` vẫn ghi T01–T12 PENDING vì log nghiệm thu M1 chưa được đưa vào repo; M1 PASS dựa trên xác nhận của người dùng |
| K21 | DOC | Các `TEST_REPORT_M2..M6.md` là bản ghi lịch sử (firmware `M2_…`–`M6_PCMON_01`); bản hiện hành là RC1 ([FINAL_QA_REPORT.md](FINAL_QA_REPORT.md)) |
| K22 | LIMITATION | Warning `TOUCH_CS pin not defined` của TFT_eSPI là bình thường (touch dùng thư viện XPT2046 riêng) |
