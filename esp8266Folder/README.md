Weather Node -- ESP8266 & BMP280

Node thu thập dữ liệu khí tượng môi trường sử dụng vi điều khiển ESP8266 và cảm biến khí áp/nhiệt độ BMP280 qua giao thức I2C, định kỳ gửi dữ liệu lên AI Server qua giao thức HTTP POST.

1. Sơ đồ kết nối chân phần cứng (Pinout)

Chân cảm biến BMP280 : Chân ESP8266 (NodeMCU) : Chức năng :
VCC : 3.3V : Nguồn cấp điện áp 3.3V (không cấp 5V) :
GND : GND : Nối mass chung :
SCL : D1 (GPIO 5) : I2C Serial Clock :
SDA : D2 (GPIO 4) : I2C Serial Data :
CSB / SDO : Không nối (hoặc nối GND để chọn địa chỉ `0x76`) : Cấu hình địa chỉ I2C :

2. Cài đặt môi trường Arduino IDE

Board Manager: Cài đặt gói `esp8266 by ESP8266 Community`.
Cấu hình Menu Tools:

- Board: `NodeMCU 0.9 (ESP-12E Module)` (hoặc loại board tương ứng).
- Upload Speed: `115200`.
  Thư viện: Cài đặt qua Arduino Library Manager:
- `Adafruit BMP280 Library`
- `Adafruit Unified Sensor`

3. Các bước nạp chương trình

Mở file `esp8266Folder.ino` trong Arduino IDE.
Chuyển sang tab `config.h`, cập nhật thông tin:

```cpp
define WIFI_SSID     "Ten_WiFi"
define WIFI_PASSWORD "Mat_Khau_WiFi"
define SERVER_HOST   "192.168.xxx" // IP LAN cua may chay AI Server
define READ_INTERVAL_MS 300000UL  // 5 phut
```

> Lưu ý: Khi kiểm thử thực tế, có thể đặt `READ_INTERVAL_MS` thành `5000UL` (5 giây) để quan sát dữ liệu gửi về server liên tục.

Kết nối board ESP8266 qua cáp Micro-USB và bấm Upload.
Mở Serial Monitor với tốc độ 9600 baud để xem thông số nhiệt độ (°C) và áp suất (hPa) được đọc và mã trạng thái HTTP trả về từ AI Server.
