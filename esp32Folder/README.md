Voice Node -- ESP32-S3 N16R8

Node giao tiếp giọng nói hai chiều sử dụng vi điều khiển ESP32-S3, tích hợp micro số I2S (INMP441) và mạch khuếch đại giải mã âm thanh I2S (MAX98357A).

1. Sơ đồ kết nối chân phần cứng (Pinout)

Micro I2S (INMP441) -- Cổng I2S_0
VDD : 3.3V : Nguồn cấp 3.3V :
GND : GND : Nối mass chung :
L/R : GND : Kênh trái (Left Channel) :
WS : GPIO 15 : Word Select (I2S Clock chọn kênh) :
SCK : GPIO 45 : Serial Clock (Bit Clock) :
SD : GPIO 35 : Serial Data Out :

Loa I2S (MAX98357A) -- Cổng I2S_1
VIN : 5V / 3.3V : Nguồn cấp cho mạch công suất :
GND : GND : Nối mass chung :
BCLK : GPIO 5 : Bit Clock :
LRC : GPIO 6 : Left/Right Clock (Word Select) :
DIN : GPIO 4 : Data In :
GAIN / SD : Không nối / GND : Mức khuếch đại mặc định :

Thiết bị ngoại vi khác
: Nút BOOT : GPIO 0 : Nhấn để kích hoạt ghi âm thủ công :
: Đèn LED : GPIO 2 / Built-in : Sáng khi đang thu âm, tắt khi ở trạng thái IDLE :

2. Cài đặt môi trường Arduino IDE

Board Manager: Cài đặt gói `esp32 by Espressif Systems` (phiên bản $\ge 2.0.11$).
Cấu hình Menu Tools:

- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled` (hoặc `Disabled` tùy loại mạch nạp)
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Upload Speed: `921600`
  Thư viện: Cài đặt thư viện `Blynk` qua Library Manager.

3. Các bước nạp chương trình

Mở file `esp32Folder.ino` trong Arduino IDE.
Chuyển sang tab `config.h`, cập nhật các thông số mạng:

```cpp
define WIFI_SSID     "Ten_WiFi"
define WIFI_PASSWORD "Mat_Khau_WiFi"
define SERVER_HOST   "192.168.xxx" // IP LAN cua AI Server
define BLYNK_AUTH_TOKEN "Token_Cua_Ban"
```

Kết nối board ESP32-S3 qua cáp Type-C và bấm Upload.
Mở Serial Monitor với tốc độ 115200 baud để quan sát tiến trình khởi động, kiểm tra loa (tiếng beep kiểm tra) và trạng thái kết nối Blynk.
