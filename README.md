Hệ thống Trợ lý ảo AI Cục bộ & Giám sát Môi trường (LEVA)

Dự án phát triển hệ thống trợ lý ảo cục bộ (Edge AI Assistant) kết hợp trạm quan trắc môi trường IoT, bảo đảm quyền riêng tư, khả năng suy luận nhanh và hoạt động theo thời gian thực.

1.  Sơ đồ kiến trúc hệ thống (System Architecture)
    Hệ thống bao gồm 3 khối thành phần chính liên kết qua mạng cục bộ (LAN) và nền tảng đám mây Blynk IoT:

        SENSOR_NODE["Weather Node (ESP8266)"]
            BMP280["Cảm biến BMP280\n(I2C: Nhiệt độ, Áp suất)"]
            ESP8266["Vi điều khiển ESP8266"]
            BMP280 -->|Đọc dữ liệu| ESP8266

        VOICE_NODE["Voice Node (ESP32-S3)"]
            MIC["Micro INMP441\n(I2S Thu âm)"]
            SPK["Loa MAX98357A\n(I2S Phát âm thanh)"]
            ESP32["Vi điều khiển ESP32-S3\n(Xử lý Ring Buffer & WiFi)"]
            MIC -->|16kHz PCM| ESP32
            ESP32 -->|WAV Stereo| SPK


        AI_SERVER["Local AI Server (FastAPI)"]
            STT["Faster-Whisper (STT)\nNhận dạng giọng nói"]
            LLM["Qwen2.5 qua Ollama (LLM)\nSuy luận & Grounding cảm biến"]
            TTS["Edge-TTS (TTS)\nTổng hợp tiếng nói tiếng Việt"]
            CACHE["Bộ nhớ đệm & Đồng bộ\nsensor_data.json"]
            STT --> LLM
            LLM --> TTS
    
        CLOUD["Blynk 2.0 Cloud & Mobile App"]
            V1["V1: Nút kích hoạt ghi âm"]
            V2["V2: Hiển thị Nhiệt độ"]
            V3["V3: Hiển thị Áp suất"]
            V4["V4: Terminal trạng thái Pipeline"]
            V5["V5: Terminal lịch sử hội thoại"]
            V6["V6: Nút hủy / Dừng khẩn cấp"]

3. Luồng kết nối
```
   ESP8266 -->|HTTP POST /sensor-update| AI_SERVER
   ESP32 -->|HTTP POST /voice-query (WAV)| AI_SERVER
   AI_SERVER -->|HTTP 200 (WAV Response)| ESP32
   AI_SERVER -->|Cập nhật V2, V3, V4, V5| CLOUD
   CLOUD <-->|Đồng bộ V1, V4, V6 (Blynk Native)| ESP32
```
5. Cấu trúc thư mục

```
DoAn/
├── serverFolder/ AI Server trung tâm (FastAPI, Whisper, Ollama, Edge-TTS)
│ ├── main.py Entrypoint REST API và bộ điều phối
│ ├── ai_pipeline.py Pipeline AI: STT, LLM Context Grounding, TTS
│ ├── blynk_client.py Thread-safe Blynk REST client
│ ├── config.py Cấu hình Token, Model AI, Port mạng
│ └── requirements.txt Danh sách thư viện Python
├── esp32Folder/ Voice Node phần cứng (ESP32-S3 N16R8)
│ ├── esp32Folder.ino Firmware Arduino thu/phát âm thanh I2S & Blynk
│ └── config.h Chân phần cứng I2S, WiFi, Server Host
└── esp8266Folder/ Weather Node phần cứng (ESP8266 + BMP280)
├── esp8266Folder.ino Firmware Arduino đọc cảm biến I2C & gửi HTTP
└── config.h Cấu hình WiFi, chu kỳ đo, Server Host
```

4.  Yêu cầu công nghệ & Thư viện sử dụng

    4.1. AI Server

- Hệ điều hành: Windows 10/11, Linux hoặc macOS.
- Python: Phiên bản 3.10 trở lên.
- Ollama Engine: Đã tải model `qwen2.5:1.5b-instruct` (`ollama run qwen2.5:1.5b-instruct`).
- FFmpeg: Đã cài đặt và thêm vào biến môi trường PATH (phục vụ `pydub` giải mã âm thanh).
- Thư viện Python (`requirements.txt`):
  `fastapi`, `uvicorn`, `faster-whisper`, `edge-tts`, `pydub`, `requests`.

  4.2. Phần cứng Voice Node (ESP32-S3)

- Vi điều khiển: ESP32-S3 Dev Module (khuyên dùng bản có PSRAM).
- Cảm biến thu âm: INMP441 (chuẩn giao tiếp I2S).
- Mạch khuếch đại phát âm: MAX98357A (chuẩn giao tiếp I2S) kèm loa 4Ω/3W.
- Thư viện Arduino: `Blynk` (bởi Volodymyr Shymanskyy), `WiFi.h`, `driver/i2s.h`.

  4.3. Phần cứng Weather Node (ESP8266)

- Vi điều khiển: ESP8266 (NodeMCU / Wemos D1 Mini).
- Cảm biến khí tượng: BMP280 (giao tiếp I2C địa chỉ `0x76` hoặc `0x77`).
- Thư viện Arduino: `Adafruit BMP280 Library`, `Adafruit Unified Sensor`, `ESP8266HTTPClient`.

5.  Các bước thực hiện để chạy đồ án

Bước 1: Khởi động Ollama & chuẩn bị mô hình LLM

Mở terminal và đảm bảo dịch vụ Ollama đang hoạt động:

````bash
ollama serve
ollama pull qwen2.5:1.5b-instruct


 Bước 2: Cài đặt và khởi chạy Local AI Server

1. Điều hướng vào thư mục server:
   ```bash
   cd serverFolder
````

2. Tạo môi trường ảo và cài đặt thư viện:
   ```bash
   python -m venv .venv
    Windows:
   .venv\Scripts\activate
    Linux/macOS:
   source .venv/bin/activate
   pip install -r requirements.txt
   ```
3. Chạy server FastAPI:
   ```bash
   python main.py
   ```
4. Kiểm tra địa chỉ IP LAN của máy chủ (ví dụ: `192.168.1.253`) bằng lệnh `ipconfig` (Windows) hoặc `ifconfig` (Linux/Mac).

Bước 3: Nạp chương trình cho Weather Node (ESP8266)

1. Mở file `esp8266Folder/esp8266Folder.ino` trong Arduino IDE.
2. Vào tab `config.h`, cập nhật thông tin:
   - `WIFI_SSID` và `WIFI_PASSWORD`.
   - `SERVER_HOST`: Điền địa chỉ IP LAN của máy chủ vừa lấy ở Bước 2.
3. Nạp code vào mạch ESP8266 và kiểm tra Serial Monitor (9600 baud) để xác nhận dữ liệu cảm biến được gửi lên server thành công.

Bước 4: Nạp chương trình cho Voice Node (ESP32-S3)

1. Mở file `esp32Folder/esp32Folder.ino` trong Arduino IDE.
2. Vào tab `config.h`, cấu hình:
   - Thông tin WiFi và `BLYNK_AUTH_TOKEN`.
   - `SERVER_HOST`: Điền IP LAN của máy chủ.
3. Chọn Board: ESP32S3 Dev Module (bật OPI PSRAM nếu có).
4. Nạp code và mở Serial Monitor (115200 baud).

Bước 5: Vận hành & Trải nghiệm

- Cách 1 (Qua App Blynk): Bấm nút V1 trên ứng dụng Blynk để bắt đầu ghi âm 5 giây. Hệ thống tự động thu âm $\rightarrow$ chuyển thành văn bản $\rightarrow$ lấy ngữ cảnh nhiệt độ/áp suất từ ESP8266 $\rightarrow$ suy luận câu trả lời $\rightarrow$ phát âm thanh qua loa.
- Cách 2 (Kiểm thử tự động):
  ```bash
  cd testModule
  python test_integration.py --auto
  ```
