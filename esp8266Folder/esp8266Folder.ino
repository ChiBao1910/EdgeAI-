/*
 * esp8266Folder.ino -- Weather Node (ESP8266 + BMP280).
 * Doc du lieu nhiet do va ap suat qua I2C, truyen HTTP POST len AI Server dinh ky.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "config.h"

Adafruit_BMP280 bmp;
bool bmpOk = false;
unsigned long lastReadAt = 0;

bool laySystemRunning() {
  // Mac dinh tra ve true neu khong hoi duoc server, de mot su co mang
  // tam thoi khong lam Weather Node ngung han viec do dac.
  if (WiFi.status() != WL_CONNECTED) return true;
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SYSTEM_STATUS_PATH;
  http.begin(client, url);
  int code = http.GET();
  bool running = true;
  if (code == 200) {
    String body = http.getString();
    running = body.indexOf("\"running\":true") >= 0 || body.indexOf("\"running\": true") >= 0;
  }
  http.end();
  return running;
}

void baoLoiChoServer(const String& msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SENSOR_ERROR_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  String json = "{\"message\":\"" + msg + "\"}";
  http.POST(json);
  http.end();
}

void guiDuLieuChoServer(float t, float p) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + SENSOR_UPDATE_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  String json = "{\"temperature_c\":" + String(t, 1) + ",\"pressure_hpa\":" + String(p, 1) + "}";
  int code = http.POST(json);
  Serial.print(F("=> Gui AI Server, ma HTTP: ")); Serial.println(code);
  http.end();
}

void doVaGuiDuLieu() {
  if (!bmpOk) {
    Serial.println(F("Khong the doc du lieu tu BMP280!"));
    baoLoiChoServer("Mat ket noi cam bien BMP280");
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);
  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0F;

  if (isnan(t) || isnan(p)) {
    Serial.println(F("Doc cam bien tra ve gia tri khong hop le!"));
    baoLoiChoServer("Doc cam bien BMP280 tra ve gia tri khong hop le");
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  Serial.println(F("--- Ket qua do ---"));
  Serial.print(F("Nhiet do: ")); Serial.print(t); Serial.println(" *C");
  Serial.print(F("Ap suat: "));  Serial.print(p); Serial.println(" hPa");

  guiDuLieuChoServer(t, p);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println(F("=== Weather Node (ESP8266) -- chi noi voi AI Server ==="));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Dang ket noi WiFi"));
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

  bmpOk = bmp.begin(0x76);
  if (!bmpOk) {
    Serial.println(F("LOI: Khong tim thay BMP280 luc khoi dong!"));
    baoLoiChoServer("Khoi dong loi: khong tim thay cam bien BMP280");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                     Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16,
                     Adafruit_BMP280::STANDBY_MS_500);
  }

  lastReadAt = millis() - READ_INTERVAL_MS; // doc ngay o lan chay dau
}

void loop() {
  if (millis() - lastReadAt >= READ_INTERVAL_MS) {
    lastReadAt = millis();
    if (laySystemRunning()) {
      Serial.println(F("\n>>> DOC VA GUI BMP280..."));
      doVaGuiDuLieu();
    } else {
      Serial.println(F("\n>>> He thong dang DUNG (V1/V6 tren Blynk) -- bo qua lan doc nay."));
    }
  }
}
