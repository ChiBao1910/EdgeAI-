#pragma once
/*
 * config.example.h -- Voice Node (ESP32-S3 N16R8)
 *
 * HUONG DAN:
 *   1. Sao chep file nay thanh config.h (cung thu muc)
 *   2. Dien gia tri THUC TE vao cac o YOUR_...
 *   3. TUYET DOI khong commit config.h len Git (da co trong .gitignore)
 *
 * ESP32-S3 ket noi TRUC TIEP voi Blynk (device hien Online).
 * V1 / V6 duoc xu ly ngay tai ESP32 qua BLYNK_WRITE handler.
 * AI Server (serverFolder) chi nhan /voice-query va cap nhat V4/V5 bang REST.
 *
 * QUAN TRONG: 3 dong BLYNK_TEMPLATE_ID / NAME / AUTH_TOKEN phai xuat hien
 * TRUOC bat ky #include <Blynk...> nao -- do do chung duoc dat trong config.h
 * va config.h duoc include DAU TIEN trong .ino.
 */

// ==== Blynk 2.0 (PHAI DUNG TRUOC #include <BlynkSimpleEsp32.h>) ====
// Lay tai: https://blynk.cloud → Template → Template ID & Auth Token
// Hien log Blynk len Serial Monitor -- rat huu ich khi debug ket noi
#define BLYNK_PRINT Serial
#define BLYNK_HEARTBEAT     30  // Giu ket noi Blynk on dinh (30 giay), chong rot mang do delay
#define BLYNK_TEMPLATE_ID   "YOUR_BLYNK_TEMPLATE_ID"    // VD: "TMPL6k2kuJ1Tb"
#define BLYNK_TEMPLATE_NAME "YOUR_BLYNK_TEMPLATE_NAME"  // VD: "LEVA"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_AUTH_TOKEN"      // VD: "dLA6XSZ5aeIql4..."

// ==== WiFi ====
#define WIFI_SSID     "YOUR_WIFI_SSID"      // Ten mang WiFi
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Mat khau WiFi

// ==== AI Server (chi HTTP thuan, khong qua Blynk) ====
// Doi thanh IP LAN THUC TE cua may dang chay serverFolder/main.py
// Xem IP bang lenh: ipconfig (Windows) hoac ip addr (Linux/Mac)
#define SERVER_HOST "YOUR_SERVER_LAN_IP"  // VD: "192.168.1.100"
#define SERVER_PORT 8000
#define SERVER_PATH "/voice-query"

// ==== I2S Microphone (VD: INMP441) ====
#define I2S_MIC_WS   15
#define I2S_MIC_SD   35
#define I2S_MIC_SCK  45
#define I2S_MIC_PORT I2S_NUM_0

// ==== I2S Speaker (VD: MAX98357A) ====
#define I2S_SPK_BCLK 5
#define I2S_SPK_LRC  6
#define I2S_SPK_DIN  4
#define I2S_SPK_PORT I2S_NUM_1

// ==== Nut bam & thong so ghi am & LED ====
#define BUTTON_PIN     0            // nut BOOT tren board
#ifndef LED_PIN
  #ifdef LED_BUILTIN
    #define LED_PIN LED_BUILTIN
  #else
    #define LED_PIN 2               // Chan LED bao trang thai (sang khi ghi am)
  #endif
#endif
#define SAMPLE_RATE    16000
#define RECORD_SECONDS 5
#define SAMPLE_COUNT   (SAMPLE_RATE * RECORD_SECONDS)

// ==== Ban do Blynk Virtual Pin ====
#define VPIN_START_STOP V1   // Blynk Button: Khoi dong / Tiep tuc ghi am
#define VPIN_STATUS     V4   // Blynk Terminal: Trang thai AI pipeline
#define VPIN_HISTORY    V5   // Blynk Terminal: Lich su tro chuyen
#define VPIN_EMERGENCY  V6   // Blynk Button: Dung khan cap / Dung he thong
