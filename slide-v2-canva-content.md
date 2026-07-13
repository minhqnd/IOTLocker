# Noi dung slide Canva - IOT102 v2 RFID Locker

## Slide 1 - Bia
TỦ GỬI ĐỒ TỰ PHỤC VỤ RFID

Arduino Uno + ESP32 + Keypad A/B/C + Dashboard IoT

Môn: IOT102
Nhóm: [Tên nhóm]
Người trình bày: [Tên thành viên]
Ngày báo cáo: [Ngày báo cáo]

Ghi chú nói:
Đề tài của nhóm là tủ gửi đồ tự phục vụ. Người dùng tự gửi và tự lấy đồ bằng thẻ RFID, chọn thao tác bằng keypad. Hệ thống ưu tiên chạy local để khi mất mạng vẫn dùng được.

## Slide 2 - Noi dung bao cao
NỘI DUNG BÁO CÁO

1. Mục tiêu và lý do chọn đề tài
2. Sơ đồ khối hệ thống
3. Linh kiện chính và sơ đồ chân
4. Nguyên lý hoạt động A/B/C
5. Lưu đồ thuật toán
6. Demo, kiểm thử và kết quả
7. Hạn chế, hướng phát triển, Q&A

Ghi chú nói:
Em sẽ đi từ mục tiêu, sang kiến trúc phần cứng, sau đó giải thích thuật toán và phần IoT/server.

## Slide 3 - Gioi thieu chung
GIỚI THIỆU ĐỀ TÀI

Vấn đề:
- Tủ gửi đồ truyền thống cần chìa khóa hoặc nhân viên hỗ trợ.
- Nếu dùng SMS/PIN/online hoàn toàn thì demo dễ lỗi khi mất mạng.
- Đồ án cần thể hiện rõ cảm biến, cơ cấu chấp hành, xử lý trung tâm và IoT.

Giải pháp:
- Dùng RFID làm chìa khóa định danh.
- Dùng keypad A/B/C để chọn thao tác.
- ESP32 tự quyết định mở hộc bằng dữ liệu lưu local.
- Server chỉ dùng để log, dashboard và xử lý phí quá hạn.

Ghi chú nói:
Điểm chính của v2 là đơn giản hóa. Thẻ RFID chỉ cho biết user là ai, còn keypad cho biết user muốn làm gì.

## Slide 4 - Muc tieu he thong
MỤC TIÊU HỆ THỐNG

Mục tiêu chính:
- Cho phép người dùng tự gửi và tự lấy đồ.
- Mỗi thẻ RFID chỉ giữ 1 hộc tại một thời điểm.
- Tủ tự chọn hộc trống đầu tiên khi gửi.
- Mất mạng vẫn gửi/lấy được bằng local storage.
- Có dashboard theo dõi trạng thái hộc và lịch sử thao tác.

Tiêu chí thành công:
- Đọc UID RFID ổn định.
- Uno gửi MODE + UID sang ESP32 qua UART.
- ESP32 mở đúng servo và cập nhật trạng thái.
- Reed switch xác nhận cửa đã đóng.

Ghi chú nói:
Ở đây nhóm tập trung vào hệ thống local-first. Server là phần cộng điểm IoT, không phải điều kiện bắt buộc để mở tủ.

## Slide 5 - So do khoi
SƠ ĐỒ KHỐI HỆ THỐNG

Input:
- Keypad 4x4: chọn A/B/C
- RFID reader: đọc UID thẻ
- Reed switch: kiểm tra cửa đóng/mở

Xử lý:
- Arduino Uno: đọc keypad, LCD, RFID
- ESP32: xử lý logic hộc, NVS, Wi-Fi, servo

Output:
- Servo: mở khóa hộc
- LCD 16x2: hướng dẫn thao tác
- OLED: icon/trạng thái/QR khi cần
- Dashboard web: trạng thái và log

Luồng chính:
Keypad + RFID -> Uno -> UART -> ESP32 -> Servo/Reed/OLED -> Server

Ghi chú nói:
Uno lo giao tiếp người dùng vì có keypad và LCD. ESP32 là não chính vì có Wi-Fi, NVS và điều khiển servo.

## Slide 6 - Linh kien va vai tro
LINH KIỆN CHÍNH

Arduino Uno:
- Đọc keypad A/B/C
- Đọc UID từ module RFID
- Hiển thị hướng dẫn trên LCD
- Gửi MODE + UID cho ESP32

ESP32:
- Lưu UID tương ứng với hộc trong NVS
- Điều khiển servo mở hộc
- Đọc reed switch
- Gửi log/dữ liệu lên server khi có Wi-Fi

Module khác:
- RFID 125 kHz HW-898-A hoặc RDM6300: đọc UID
- LCD I2C: màn hình chữ
- OLED: trạng thái/QR/icon
- Servo + reed switch: khóa và xác nhận cửa

Ghi chú nói:
Nhóm chỉ dùng UID của thẻ, không ghi dữ liệu vào thẻ. Điều này giúp demo đơn giản và phù hợp môn IOT102.

## Slide 7 - So do chan va giao tiep
SƠ ĐỒ CHÂN & GIAO TIẾP

Arduino Uno:
- Keypad R1-R4: D2-D5
- Keypad C1-C4: D6-D9
- RFID TX: D10
- LCD I2C: A4/A5
- UART sang ESP32: A2/A3

ESP32:
- UART2 RX/TX: GPIO16/GPIO17
- OLED I2C: GPIO21/GPIO22
- Servo hộc 1/2: GPIO25/GPIO26
- Reed hộc 1/2: GPIO32/GPIO33

Lưu ý nguồn:
- Servo dùng nguồn 5V riêng tối thiểu 2A.
- GND của Uno, ESP32 và nguồn servo phải nối chung.
- Tín hiệu Uno A2 -> ESP32 GPIO16 cần chia áp 5V xuống 3.3V.

Ghi chú nói:
Đây là phần hội đồng thường hỏi kỹ. Điểm quan trọng là ESP32 không chịu được 5V logic, nên chiều Uno sang ESP32 phải chia áp.

## Slide 8 - Nguyen ly hoat dong ABC
NGUYÊN LÝ HOẠT ĐỘNG A/B/C

A - Gửi đồ:
- Người dùng bấm A.
- LCD yêu cầu quẹt thẻ.
- ESP32 tìm hộc trống đầu tiên.
- Gán UID với hộc và mở servo.
- Reed xác nhận đóng cửa rồi lưu trạng thái đầy.

B - Gửi thêm:
- Người dùng bấm B và quẹt lại thẻ.
- ESP32 tìm UID đang giữ hộc nào.
- Mở đúng hộc đó.
- Không xóa mapping UID-hộc.

C - Lấy đồ:
- Người dùng bấm C và quẹt thẻ.
- ESP32 kiểm tra UID.
- Mở đúng hộc.
- Sau khi đóng cửa, xóa mapping để hộc trống lại.

Ghi chú nói:
Keypad là phần rất quan trọng vì RFID chỉ định danh người dùng, không thể biết user muốn gửi hay lấy.

## Slide 9 - Luu do thuat toan
LƯU ĐỒ THUẬT TOÁN

Bước 1: Chờ người dùng bấm A/B/C
Bước 2: LCD hiển thị "Quẹt thẻ"
Bước 3: Uno đọc UID RFID
Bước 4: Uno gửi MODE + UID sang ESP32
Bước 5: ESP32 phân nhánh:
- MODE A: UID chưa giữ hộc -> chọn hộc trống -> mở
- MODE B: UID đã giữ hộc -> mở hộc hiện tại
- MODE C: UID đã giữ hộc -> mở -> xóa mapping
Bước 6: Reed xác nhận cửa đóng
Bước 7: Lưu NVS và gửi log khi có mạng

Ghi chú nói:
Nếu vẽ lại trên Canva, nên dùng 1 flowchart lớn ở giữa, ba nhánh A/B/C tô màu khác nhau.

## Slide 10 - IoT va server
PHẦN IOT / SERVER

Vai trò server:
- Nhận event gửi/lấy đồ.
- Lưu lịch sử quẹt thẻ.
- Hiển thị dashboard trống/đầy/quá hạn.
- Xử lý phí quá hạn khi có mạng.

API dự kiến:
- POST /api/deposit: log gửi đồ
- POST /api/pickup: log lấy đồ
- POST /api/heartbeat: tủ báo còn online
- GET /api/payment-status: kiểm tra thanh toán
- Webhook SePay: xác nhận đã thanh toán

Nguyên tắc:
- Local quyết định mở hộc.
- Server hỗ trợ giám sát và thanh toán.
- Mất mạng thì lưu log tạm, gửi bù khi online.

Ghi chú nói:
Điểm IoT nằm ở dashboard, heartbeat, event log và xử lý dữ liệu quá hạn, không phải phụ thuộc mạng để mở khóa.

## Slide 11 - Phi qua han va offline fallback
PHÍ QUÁ HẠN & DỰ PHÒNG

Chính sách phí:
- 30 phút đầu miễn phí.
- Sau đó thu 5.000đ.

Khi có mạng:
- Nếu hộc quá hạn, người dùng phải thanh toán trước khi lấy.
- Lựa chọn 1: QR SePay, trả xong tủ tự mở.
- Lựa chọn 2: trả khi lấy xe, chỉ áp dụng nếu UID là thẻ xe.

Khi mất mạng:
- Bấm C, quẹt đúng thẻ thì vẫn mở.
- Bỏ qua phí tạm thời để tránh kẹt đồ.
- Log được gửi bù khi có mạng.

Ghi chú nói:
Fallback offline là quyết định thiết kế. Trong đồ án demo, ưu tiên trải nghiệm không bị khóa vì mất mạng.

## Slide 12 - Demo va kiem thu
DEMO & KIỂM THỬ

Kịch bản demo:
1. Bấm A -> quẹt RFID -> tủ chọn hộc trống -> servo mở.
2. Đóng cửa -> reed xác nhận -> dashboard cập nhật đầy.
3. Bấm B -> quẹt cùng thẻ -> mở lại đúng hộc.
4. Bấm C -> quẹt thẻ -> mở hộc -> xóa trạng thái.
5. Ngắt Wi-Fi -> kiểm tra tủ vẫn gửi/lấy local.

Bảng kiểm thử:
- RFID đọc UID: [Đạt/Chưa]
- UART Uno-ESP32: [Đạt/Chưa]
- Servo mở đúng hộc: [Đạt/Chưa]
- Reed xác nhận cửa: [Đạt/Chưa]
- Dashboard nhận log: [Đạt/Chưa]
- Offline fallback: [Đạt/Chưa]

Ghi chú nói:
Slide này nên chèn ảnh thật hoặc video ngắn. Nếu chưa quay được, dùng bảng test này để chứng minh kế hoạch kiểm thử rõ ràng.

## Slide 13 - Ket luan va Q&A
KẾT LUẬN & Q&A

Kết quả đạt được:
- Thiết kế được hệ thống tủ gửi đồ RFID local-first.
- Phân chia rõ vai trò Uno, ESP32, cảm biến và server.
- Có 3 luồng thao tác A/B/C dễ demo.
- Có phương án mất mạng và an toàn vận hành.

Hạn chế:
- UID RFID có thể bị clone.
- Cần test thực tế độ ổn định nguồn servo.
- Phần thanh toán/quá hạn phụ thuộc server khi online.

Hướng phát triển:
- Thêm master card quản trị.
- Thêm dashboard đẹp hơn.
- Thêm thông báo Telegram khi quá hạn.
- Mở rộng nhiều hộc hơn.

CẢM ƠN THẦY VÀ CÁC BẠN

Ghi chú nói:
Khi Q&A, nhấn mạnh nhóm hiểu rõ giới hạn của UID-only và đã có phương án dự phòng bằng master card/quầy dịch vụ.
