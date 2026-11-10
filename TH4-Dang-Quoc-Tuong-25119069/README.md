# TH4 – REST/MQTT và dashboard Node-RED

- Họ tên: Dang Quoc Tuong
- MSSV: 25119069

## 1. Nội dung bài làm

Bài thực hành gồm phần đọc dữ liệu từ ESP32 bằng REST, gửi dữ liệu qua MQTT và điều khiển thiết bị bằng dashboard Node-RED. Em chia mã nguồn thành 3 project PlatformIO để nạp và kiểm tra từng bài.

| Thư mục | Nội dung |
|---|---|
| `bai1-rest` | Tạo API `GET /api/sensor`, trả về JSON gồm nhiệt độ, độ ẩm, uptime và các trường bổ sung `id`, `rssi` |
| `bai2-mqtt-pub` | Gửi dữ liệu lên MQTT broker theo chu kỳ 2 giây, dùng topic theo MSSV |
| `bai3-mqtt-sub-led` | Kết hợp cảm biến, nút nhấn, LED và OLED; nhận lệnh điều khiển từ dashboard |
| `nodered-flow` | File `flow.json` để import vào Node-RED |
| `docs` | Hướng dẫn nối dây, chạy chương trình và ôn tập vấn đáp |
| `media` | Ảnh/video  và thư mục dành cho kết quả thực nghiệm |

Bài 4 sử dụng lại chương trình bài 2 hoặc bài 3 để kiểm tra bằng MQTT Explorer, không có firmware riêng.

## 2. Phần cứng và cấu hình

Chương trình được cấu hình cho ESP32 DevKit, cảm biến SHT31, OLED SSD1306 128×64, một LED kèm điện trở 220 Ω và một nút nhấn.

| Thiết bị | Chân kết nối |
|---|---|
| SHT31 và OLED | SDA → GPIO21, SCL → GPIO22; nguồn 3V3 và GND |
| LED | GPIO14 → LED → điện trở 220 Ω → GND |
| Nút nhấn | GPIO27 → nút → GND, dùng `INPUT_PULLUP` |

Địa chỉ I2C mặc định của SHT31 là `0x44`, OLED là `0x3C`. Chi tiết nằm trong [bảng nối dây](docs/wiring.md).

Broker dùng trong cấu hình là `broker.hivemq.com`, cổng `1883`. ESP32, Node-RED và MQTT Explorer cần kết nối cùng broker.

| Topic | Chức năng |
|---|---|
| `aet2022/25119069/telemetry` | Gửi số đo và trạng thái thiết bị |
| `aet2022/25119069/cmd/led` | Nhận `1` để bật LED, `0` để tắt LED |
| `aet2022/25119069/status` | Báo trạng thái `online` hoặc `offline` |

## 3. Cách chạy

1. Nhập Wi-Fi bằng lệnh sau tại thư mục gốc:

   ```bash
   python tools/setup_config.py --wifi
   ```

   Thông tin Wi-Fi được lưu trong `secrets.h`, không đưa lên repo.

2. Mở thư mục bài cần chạy bằng PlatformIO, chọn **Build → Upload**. Mở Serial Monitor ở tốc độ **115200 baud**. Mỗi lần nạp một bài lên ESP32.
3. Với bài 1, lấy IP trên Serial Monitor rồi mở `http://<IP>/api/sensor` để xem JSON.
4. Với bài 2, mở MQTT Explorer và theo dõi topic `aet2022/25119069/telemetry`.
5. Với bài 3, cài gói `node-red-dashboard`, import `nodered-flow/flow.json`, chọn **Deploy** rồi mở `http://localhost:1880/ui`.
6. Bấm BẬT/TẮT trên dashboard để kiểm tra LED và OLED. Bấm nút trên kit để kiểm tra LED đảo trạng thái và dữ liệu cập nhật lên dashboard.
7. Với bài 4, xem cây topic và payload trong MQTT Explorer; kiểm tra `cmd/led` có giá trị `1` hoặc `0` khi bấm nút trên dashboard.

Dashboard có gauge nhiệt độ, độ ẩm, biểu đồ và nút điều khiển LED. Phần bổ sung gồm RSSI, uptime, trạng thái kết nối và trạng thái nút nhấn.

Khi chưa có cảm biến, có thể đặt `SENSOR_MODE` thành `SENSOR_SIM` trong `include/config.h` để thử phần truyền dữ liệu của bài 1 và 2. Khi đó số đo là mô phỏng và JSON có trường `"sensor":"sim"`.

## 4. Tiến độ và kết quả

Repo hiện có mã nguồn 3 bài và flow Node-RED. Phần chạy trên kit chưa có kết quả thực nghiệm được xác nhận; IP ESP32 và ảnh/video chạy thực tế còn cần bổ sung vào `media/results`.

Các ảnh/video trong `media/illustrations` dùng để minh họa giao diện và cách hoạt động.

## 5. Nhận xét

Qua phần chương trình, em hiểu REST hoạt động theo cách client gửi yêu cầu rồi ESP32 trả dữ liệu. MQTT cho phép ESP32 chủ động gửi dữ liệu qua broker, phù hợp với việc cập nhật dashboard liên tục. Dashboard cũng có thể gửi lệnh ngược lại để điều khiển LED.

Việc đặt topic theo MSSV giúp phân biệt dữ liệu của từng sinh viên khi dùng chung broker. Trạng thái LED trên dashboard được lấy từ dữ liệu ESP32 gửi về để đối chiếu với lệnh đã gửi.

Hướng dẫn chi tiết: [Cách chạy](docs/run-guide.md) · [Checklist nộp bài](docs/submission-checklist.md).
