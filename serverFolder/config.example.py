"""
HUONG DAN:
  1. Sao chep file nay: cp config.example.py config.py
  2. Mo config.py va dien gia tri THUC TE vao cac o "YOUR_..."
"""

# ==== Blynk Cloud Configuration ====
# Lay tai: https://blynk.cloud → Template → Template ID & Auth Token
BLYNK_TEMPLATE_ID = "YOUR_BLYNK_TEMPLATE_ID"        # VD: "TMPL6k2kuJ1Tb"
BLYNK_TEMPLATE_NAME = "YOUR_BLYNK_TEMPLATE_NAME"    # VD: "LEVA"
BLYNK_AUTH_TOKEN = "YOUR_BLYNK_AUTH_TOKEN"          # VD: "dLA6XSZ5aeIql4..."
BLYNK_BASE = "https://blynk.cloud/external/api"
BLYNK_MIN_INTERVAL = 0.4  # Khoang cach toi thieu giua 2 lan goi REST (chong rate limit)

# ==== Blynk Virtual Pin Mapping ====
PIN_START_STOP = "v1"     # Button: Khoi dong / Tiep tuc ghi am
PIN_TEMPERATURE = "v2"    # Value: Nhiet do (ESP8266 BMP280)
PIN_PRESSURE = "v3"       # Value: Ap suat (ESP8266 BMP280)
PIN_STATUS = "v4"         # Terminal: Trang thai he thong & AI Pipeline
PIN_HISTORY = "v5"        # Terminal: Lich su hoi thoai
PIN_EMERGENCY = "v6"      # Button: Dung khan cap

# ==== Network & Directories ====
HOST = "0.0.0.0"
PORT = 8000
SAVE_DIR = "records"
ANSWER_DIR = "answers"

# ==== AI Models & Audio Parameters ====
WHISPER_MODEL_SIZE = "medium"          # Model Faster-Whisper
OLLAMA_URL = "http://127.0.0.1:11434/api/chat"
QWEN_MODEL = "qwen2.5:1.5b-instruct"   # Model LLM chay qua Ollama
TTS_VOICE = "vi-VN-NamMinhNeural"      # Giong doc Edge-TTS tieng Viet
TTS_RATE = "+15%"
TTS_VOLUME = "+50%"
TTS_PITCH = "+0Hz"
