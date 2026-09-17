/*
 * esp32Folder.ino -- Voice Node (ESP32-S3 N16R8).
 * Thu am qua I2S INMP441, truyen HTTP toi AI Server va phat dap thoai qua I2S MAX98357A.
 * Ket noi Blynk Cloud dong bo dieu khien (V1: Ghi am, V4: Trang thai, V6: Huy thao tac).
 */


// config.h phai duoc include DAU TIEN (chua Template ID, Name, Auth Token)
#include "config.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <driver/i2s.h>
#include <math.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Dinh nghia trang thai he thong
// ---------------------------------------------------------------------------
enum SystemState {
  ST_BOOTING,
  ST_IDLE,
  ST_RECORDING,
  ST_SENDING,
  ST_WAITING_SERVER,
  ST_PLAYING
};

static volatile SystemState currentState = ST_BOOTING;
static volatile bool recordRequested = false;
static volatile bool cancelRequested = false;

// Buffer ghi am (16-bit PCM, 5 giay @ 16kHz ~ 80,000 mau ~ 160KB)
int16_t audioBuffer[SAMPLE_COUNT];

// ---------------------------------------------------------------------------
// Helper: Lay thoi gian thuc [HH:MM:SS] (NTP sync hoac fallback uptime)
// ---------------------------------------------------------------------------
String getTimeStr() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 50)) {
    char buf[16];
    strftime(buf, sizeof(buf), "[%H:%M:%S]", &timeinfo);
    return String(buf);
  }
  unsigned long s = millis() / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu:%02lu]", (s / 3600) % 24, (s / 60) % 60, s % 60);
  return String(buf);
}

// ---------------------------------------------------------------------------
// Helper: Gui trang thai len Blynk V4 va Serial voi mui ten ↓
// ---------------------------------------------------------------------------
void sendV4State(const String& stateName, const String& extra = "", bool showArrow = true) {
  String t = getTimeStr();
  String msg = t + stateName;
  if (extra.length() > 0) {
    msg += " (" + extra + ")";
  }
  if (showArrow) {
    msg += "\n  ↓\n";
  } else {
    msg += "\n";
  }
  Serial.print(msg);
  Blynk.virtualWrite(VPIN_STATUS, msg);
}

// ---------------------------------------------------------------------------
// Helper: Delay co goi Blynk.run() va thoat ngay neu cancelRequested
// ---------------------------------------------------------------------------
void blynkDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    Blynk.run();
    if (cancelRequested) break;
    delay(5);
  }
}

// ---------------------------------------------------------------------------
// Helper: Bat / Tat den LED bao trang thai
// ---------------------------------------------------------------------------
void ledOn() {
  digitalWrite(LED_PIN, HIGH);
}

void ledOff() {
  digitalWrite(LED_PIN, LOW);
}

// ---------------------------------------------------------------------------
// returnToIdle: Dua he thong ve IDLE, xoa sach co, san sang nhan lenh moi
// ---------------------------------------------------------------------------
void returnToIdle(const String& reason = "") {
  currentState    = ST_IDLE;
  recordRequested = false;
  cancelRequested = false;
  ledOff(); // Tat den LED khi ve IDLE

  Blynk.virtualWrite(VPIN_START_STOP, 0);
  Blynk.virtualWrite(VPIN_EMERGENCY,  0);

  if (reason.length() > 0) {
    sendV4State("IDLE", reason, false);
  } else {
    sendV4State("IDLE", "", false);
  }
}

// ---------------------------------------------------------------------------
// Tu dong duy tri ket noi WiFi / Blynk
// ---------------------------------------------------------------------------
void ensureConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Mat ket noi! Dang ket noi lai...");
    WiFi.reconnect();
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t < 4000)) {
      delay(50);
    }
  }
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
    Blynk.connect(2000);
  }
}

// ---------------------------------------------------------------------------
// Khuech dai am thanh PCM 16-bit stereo (to nhat co the, chong vo tieng)
// ---------------------------------------------------------------------------
void amplifyBuffer(uint8_t* buf, int len, float gain) {
  int16_t* samples = (int16_t*)buf;
  int count = len / 2; // 2 byte per sample (16-bit)
  for (int i = 0; i < count; i++) {
    int32_t val = (int32_t)(samples[i] * gain);
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    samples[i] = (int16_t)val;
  }
}

// ---------------------------------------------------------------------------
// BLYNK_WRITE handlers
// ---------------------------------------------------------------------------
BLYNK_WRITE(VPIN_START_STOP) {  // V1: Ghi am
  int val = param.asInt();
  Serial.printf("[Blynk] Nhan V1 = %d (State=%d)\n", val, (int)currentState);

  // CHI CHAP NHAN KHI DANG O IDLE (chong tu ghi luc boot/dang xu ly)
  if (val == 1) {
    if (currentState == ST_IDLE) {
      recordRequested = true;
      Serial.println("[Blynk] V1: Da chap nhan yeu cau ghi am!");
    } else {
      Serial.println("[Blynk] V1: He thong dang ban hoac dang khoi dong, bo qua.");
      Blynk.virtualWrite(VPIN_START_STOP, 0);
    }
  }
}

BLYNK_WRITE(VPIN_EMERGENCY) {   // V6: HUY THAO TAC & VE IDLE NGAY
  int val = param.asInt();
  Serial.printf("[Blynk] Nhan V6 = %d\n", val);
  if (val == 1) {
    cancelRequested = true;
    recordRequested = false;
    Serial.println("[Blynk] V6: YEU CAU HUY THAO TAC -> VE IDLE!");
    Blynk.virtualWrite(VPIN_EMERGENCY, 0);
    Blynk.virtualWrite(VPIN_START_STOP, 0);
  }
}

BLYNK_CONNECTED() {
  Serial.println("[Blynk] *** KET NOI THANH CONG *** Device: ONLINE");
  Blynk.virtualWrite(VPIN_START_STOP, 0);
  Blynk.virtualWrite(VPIN_EMERGENCY,  0);
  recordRequested = false;
  cancelRequested = false;
}

// ---------------------------------------------------------------------------
// I2S Microphone setup (INMP441) - I2S_NUM_0
// ---------------------------------------------------------------------------
void setupMic() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 1024
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_MIC_SCK, .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_MIC_SD
  };
  esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &cfg, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("LOI khoi tao I2S Mic: %d\n", err);
  }
  i2s_set_pin(I2S_MIC_PORT, &pins);
  Serial.println("[I2S] Mic khoi tao OK.");
}

// ---------------------------------------------------------------------------
// I2S Speaker setup (MAX98357A) - I2S_NUM_1
// ---------------------------------------------------------------------------
void setupSpeaker() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // MAX98357A nhan stereo frame
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SPK_BCLK, .ws_io_num = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DIN, .data_in_num = I2S_PIN_NO_CHANGE
  };
  esp_err_t err = i2s_driver_install(I2S_SPK_PORT, &cfg, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("LOI khoi tao I2S Speaker: %d\n", err);
  }
  i2s_set_pin(I2S_SPK_PORT, &pins);
  i2s_zero_dma_buffer(I2S_SPK_PORT);
  Serial.println("[I2S] Speaker khoi tao OK.");
}

// ---------------------------------------------------------------------------
// Phat tieng beep ngan kiem tra loa MAX98357A luc khoi dong
// ---------------------------------------------------------------------------
void playTestBeep(int freq = 880, int durationMs = 250) {
  Serial.printf("[Loa] Phat tieng beep kiem tra (%dHz %dms)...\n", freq, durationMs);
  i2s_zero_dma_buffer(I2S_SPK_PORT);
  i2s_start(I2S_SPK_PORT);

  int totalSamples = (SAMPLE_RATE * durationMs) / 1000;
  int16_t frame[2]; // L + R
  size_t written = 0;

  for (int i = 0; i < totalSamples; i++) {
    int16_t sample = (int16_t)(25000.0f * sinf(2.0f * M_PI * freq * i / SAMPLE_RATE));
    frame[0] = sample; // Left
    frame[1] = sample; // Right
    i2s_write(I2S_SPK_PORT, frame, sizeof(frame), &written, pdMS_TO_TICKS(50));
  }

  delay(50);
  i2s_zero_dma_buffer(I2S_SPK_PORT);
  Serial.println("[Loa] Da phat xong beep kiem tra.");
}

// ---------------------------------------------------------------------------
// Ghi am vao audioBuffer (co kiem tra V6 de ngat ngay va bat LED bao)
// ---------------------------------------------------------------------------
int recordAudio() {
  sendV4State("RECORDING");
  unsigned long tRecStart = millis();

  ledOn(); // BAT DEN LED KHI GHI AM DE NGUOI DUNG BIET

  i2s_zero_dma_buffer(I2S_MIC_PORT);
  int32_t raw[256];
  size_t bytesRead = 0;
  int idx = 0;
  unsigned long tStart = millis();
  unsigned long lastBlynkRun = millis();

  while (idx < SAMPLE_COUNT && (millis() - tStart < (RECORD_SECONDS + 2) * 1000UL)) {
    // Kiem tra nut V6 huy bo
    if (cancelRequested) {
      Serial.println(">>> V6: Huy ghi am!");
      ledOff();
      return -1;
    }

    esp_err_t err = i2s_read(I2S_MIC_PORT, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(50));
    if (err == ESP_OK && bytesRead > 0) {
      int n = bytesRead / sizeof(int32_t);
      for (int i = 0; i < n && idx < SAMPLE_COUNT; i++) {
        audioBuffer[idx++] = (int16_t)(raw[i] >> 14);
      }
    } else {
      delay(2);
    }

    if (millis() - lastBlynkRun > 150) {
      Blynk.run();
      lastBlynkRun = millis();
    }
  }

  ledOff(); // TAT DEN LED KHI GHI AM XONG
  float recSec = (millis() - tRecStart) / 1000.0f;
  Serial.printf(">>> Ghi am xong: %d mau (%.1fs).\n", idx, recSec);
  return idx;
}

// ---------------------------------------------------------------------------
// Tao WAV header 44 byte mono 16-bit
// ---------------------------------------------------------------------------
void writeWavHeader(uint8_t* h, uint32_t dataSize) {
  uint32_t fileSize = dataSize + 36, byteRate = SAMPLE_RATE * 2, sr = SAMPLE_RATE, sub1 = 16;
  uint16_t fmt = 1, ch = 1, align = 2, bits = 16;
  memcpy(h, "RIFF", 4); memcpy(h+4, &fileSize, 4); memcpy(h+8, "WAVEfmt ", 8);
  memcpy(h+16, &sub1, 4); memcpy(h+20, &fmt, 2); memcpy(h+22, &ch, 2);
  memcpy(h+24, &sr, 4); memcpy(h+28, &byteRate, 4); memcpy(h+32, &align, 2);
  memcpy(h+34, &bits, 2); memcpy(h+36, "data", 4); memcpy(h+40, &dataSize, 4);
}

// ---------------------------------------------------------------------------
// Helper: So sanh khong phan biet hoa thuong
// ---------------------------------------------------------------------------
bool startsWithIgnoreCase(const String& s, const char* prefix) {
  String lower = s; lower.toLowerCase();
  String pLower = String(prefix); pLower.toLowerCase();
  return lower.startsWith(pLower);
}

// ---------------------------------------------------------------------------
// Gui audio len AI Server va phat phan hoi qua loa
// ---------------------------------------------------------------------------
void sendAndPlay(int samples) {
  if (cancelRequested || samples == -1) {
    return;
  }

  if (samples <= 0) {
    Serial.println("LOI: Khong thu duoc am thanh tu mic.");
    sendV4State("IDLE", "Loi Mic", false);
    blynkDelay(1500);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("LOI: Mat ket noi WiFi.");
    sendV4State("IDLE", "Loi WiFi", false);
    WiFi.reconnect();
    blynkDelay(1500);
    return;
  }

  currentState = ST_SENDING;
  WiFiClient client;
  client.setTimeout(15);
  Serial.printf(">>> Ket noi AI Server (%s:%d)...\n", SERVER_HOST, SERVER_PORT);

  if (!client.connect(SERVER_HOST, SERVER_PORT, 4000)) {
    Serial.println("LOI: Khong ket noi duoc AI Server.");
    sendV4State("IDLE", "Loi Server", false);
    blynkDelay(1500);
    return;
  }

  uint32_t dataSize = samples * sizeof(int16_t);
  uint8_t header[44];
  writeWavHeader(header, dataSize);
  uint32_t contentLength = 44 + dataSize;

  sendV4State("SENDING", String(dataSize / 1024) + "KB");

  client.print(String("POST ") + SERVER_PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + SERVER_HOST + ":" + String(SERVER_PORT) + "\r\n");
  client.print("Content-Type: audio/wav\r\n");
  client.print(String("Content-Length: ") + String(contentLength) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  // Gui 44 byte WAV header
  client.write(header, 44);

  // Gui audio buffer theo chunk 1024 byte
  uint8_t* ptr = (uint8_t*)audioBuffer;
  size_t bytesSent = 0;
  unsigned long tSend = millis();

  while (bytesSent < dataSize && client.connected()) {
    if (cancelRequested) {
      Serial.println(">>> V6: Huy upload!");
      client.stop();
      return;
    }
    size_t toSend = min((size_t)1024, (size_t)(dataSize - bytesSent));
    size_t written = client.write(ptr + bytesSent, toSend);
    if (written > 0) {
      bytesSent += written;
    } else {
      delay(2);
    }
    Blynk.run();

    if (millis() - tSend > 20000) {
      Serial.println("LOI: Timeout upload.");
      client.stop();
      sendV4State("IDLE", "Loi Upload Timeout", false);
      blynkDelay(1500);
      return;
    }
  }

  Serial.printf(">>> Upload xong: %u byte. Cho server xu ly AI...\n", bytesSent);

  // ====== CHO SERVER PHAN HOI (TIMEOUT 60 GIAY) ======
  currentState = ST_WAITING_SERVER;
  unsigned long t0 = millis();
  while (client.connected() && !client.available() && (millis() - t0 < 60000UL)) {
    Blynk.run();
    if (cancelRequested) {
      Serial.println(">>> V6: Huy cho server phan hoi!");
      client.stop();
      return;
    }
    delay(15);
  }

  if (!client.available()) {
    Serial.println("LOI: Server timeout 60s.");
    client.stop();
    sendV4State("IDLE", "Timeout 60s", false);
    blynkDelay(1500);
    return;
  }

  // ====== DOC HTTP RESPONSE STATUS ======
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  Serial.println("HTTP Response: " + statusLine);
  if (statusLine.indexOf("200") < 0) {
    Serial.println("LOI: Server tra ve loi: " + statusLine);
    client.stop();
    sendV4State("IDLE", "Loi 500", false);
    blynkDelay(1500);
    return;
  }

  // Doc headers - tim Content-Length va X-Total-Time
  long responseLen = -1;
  String aiTotalTime = "";
  unsigned long tHdr = millis();
  while (client.connected() && (millis() - tHdr < 5000)) {
    if (!client.available()) {
      Blynk.run();
      delay(10);
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break; // Ket thuc header

    if (startsWithIgnoreCase(line, "content-length:")) {
      int colonPos = line.indexOf(':');
      if (colonPos >= 0) {
        String valStr = line.substring(colonPos + 1);
        valStr.trim();
        responseLen = valStr.toInt();
      }
    }
    if (startsWithIgnoreCase(line, "x-total-time:")) {
      int colonPos = line.indexOf(':');
      if (colonPos >= 0) {
        aiTotalTime = line.substring(colonPos + 1);
        aiTotalTime.trim();
      }
    }
  }

  if (responseLen >= 0 && responseLen < 44) {
    Serial.println("LOI: Response qua ngan (<44 bytes).");
    client.stop();
    sendV4State("IDLE", "Loi WAV ngan", false);
    blynkDelay(1500);
    return;
  }

  // Bo qua 44 byte WAV header cua phan hoi
  int skipped = 0;
  uint8_t skipbuf[44];
  unsigned long tSkip = millis();
  while (skipped < 44 && (millis() - tSkip < 5000)) {
    if (cancelRequested) { client.stop(); return; }
    if (client.available()) {
      int r = client.read(skipbuf + skipped, 44 - skipped);
      if (r > 0) skipped += r;
    } else {
      Blynk.run();
      delay(5);
    }
  }

  if (skipped < 44) {
    Serial.println("LOI: WAV header phan hoi khong du.");
    client.stop();
    sendV4State("IDLE", "Loi Header", false);
    blynkDelay(1500);
    return;
  }

  // Audio bytes thuc te can phat
  long remaining = (responseLen > 44) ? (responseLen - 44) : 999999L;

  // ====== RECEIVING_AUDIO ======
  String rxExtra = String(remaining / 1024) + "KB";
  if (aiTotalTime.length() > 0) {
    rxExtra += ", AI:" + aiTotalTime + "s";
  }
  sendV4State("RECEIVING_AUDIO", rxExtra);

  // ====== PHAT AM THANH QUA LOA MAX98357A ======
  currentState = ST_PLAYING;
  sendV4State("PLAYING");

  i2s_zero_dma_buffer(I2S_SPK_PORT);
  i2s_start(I2S_SPK_PORT);

  // Buffer co tinh can chinh boi so cua 4 byte (stereo 16-bit)
  uint8_t chunk[1024];
  int chunkFilled = 0;
  long totalPlayed = 0;
  unsigned long tPlay = millis();
  unsigned long lastBlynk = millis();
  const float GAIN = 3.5f; // Khuech dai am thanh phan mem

  while ((remaining > 0 || chunkFilled > 0) && (millis() - tPlay < 120000UL)) {
    if (millis() - lastBlynk > 150) {
      Blynk.run();
      lastBlynk = millis();
      if (cancelRequested) {
        Serial.println(">>> V6: Ngat phat am thanh loa!");
        break;
      }
    }

    // Doc them du lieu tu TCP socket neu buffer con cho
    if (client.available() > 0 && remaining > 0) {
      int space = sizeof(chunk) - chunkFilled;
      int toRead = min(space, (int)remaining);
      int r = client.read(chunk + chunkFilled, toRead);
      if (r > 0) {
        chunkFilled += r;
        remaining -= r;
      }
    } else if (!client.connected() && chunkFilled == 0) {
      break; // Da nhan het va phat het du lieu
    }

    // Chi xu ly va gui vao I2S so byte la BOI SO CUA 4 (16-bit stereo frame = 4 bytes)
    int framesBytes = (chunkFilled / 4) * 4;
    if (framesBytes > 0) {
      amplifyBuffer(chunk, framesBytes, GAIN);
      size_t written = 0;
      i2s_write(I2S_SPK_PORT, chunk, framesBytes, &written, pdMS_TO_TICKS(200));
      totalPlayed += written;

      // Di chuyen so byte du (1..3 byte) ve dau buffer
      int leftover = chunkFilled - framesBytes;
      for (int i = 0; i < leftover; i++) {
        chunk[i] = chunk[framesBytes + i];
      }
      chunkFilled = leftover;
    } else {
      delay(1);
    }
  }

  client.stop();
  delay(150); // Cho DMA I2S day het du lieu ra loa
  i2s_zero_dma_buffer(I2S_SPK_PORT);

  float playSec = (millis() - tPlay) / 1000.0f;
  Serial.printf(">>> Phat xong: %ld bytes am thanh (%.1fs).\n", totalPlayed, playSec);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n========================================");
  Serial.println("=== Voice Node (ESP32-S3) KHOI DONG ===");
  Serial.println("========================================");

  pinMode(LED_PIN, OUTPUT);
  ledOff();
  // Nhay LED nhe 1 lan de bao hieu LED hoat dong tot
  ledOn(); delay(150); ledOff();

  setupMic();
  setupSpeaker();

  // Phat tieng beep kiem tra loa (0.2s 880Hz)
  playTestBeep(880, 200);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  Serial.println("[WiFi/Blynk] Dang ket noi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  // Dong bo NTP gio thuc Viet Nam (GMT+7)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");

  // Gui trang thai khoi dong theo dung format yeu cau
  sendV4State("START");
  sendV4State("WIFI_CONNECTING");

  // QUAN TRONG: Doi 2 giay de Blynk on dinh va tieu thu het cac tin nhan sync cu
  Serial.println("[Blynk] Loc tin nhan khoi dong...");
  unsigned long tWait = millis();
  while (millis() - tWait < 2000) {
    Blynk.run();
    recordRequested = false;
    cancelRequested = false;
    delay(10);
  }

  // Thiet lap trang thai IDLE ban dau
  returnToIdle();

  Serial.println("========================================");
  Serial.println("=== KHOI DONG HOAN TAT: HE THONG SAN SANG ===");
  Serial.println("Dang cho lenh tu V1 tren Blynk de ghi am.");
  Serial.println("========================================\n");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
  Blynk.run();

  // Kiem tra ket noi WiFi/Blynk dinh ky moi 10 giay
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    ensureConnection();
    lastCheck = millis();
  }

  // Neu co tin hieu V6 khi he thong dang o IDLE -> Reset ve IDLE
  if (cancelRequested && currentState == ST_IDLE) {
    returnToIdle("V6");
  }

  // ====== KICH HOAT GHI AM TU V1 ======
  if (recordRequested && currentState == ST_IDLE && !cancelRequested) {
    recordRequested = false;
    // Reset widget V1 ve 0 ngay de nguoi dung thay nut da duoc nhan
    Blynk.virtualWrite(VPIN_START_STOP, 0);

    Serial.println("\n========================================");
    Serial.println(">>> TRIGGER GHI AM TU V1!");
    Serial.println("========================================");

    currentState = ST_RECORDING;
    int samples = recordAudio();

    if (cancelRequested || samples == -1) {
      Serial.println(">>> Ghi am bi huy boi V6.");
      returnToIdle("V6");
      return;
    }

    sendAndPlay(samples);

    if (cancelRequested) {
      returnToIdle("V6");
    } else {
      returnToIdle();
    }
  }
}
