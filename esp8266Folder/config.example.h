#pragma once
/*
 * config.example.h -- Weather Node (ESP8266 + BMP280)
 *
 * HUONG DAN:
 *   1. Sao chep file nay thanh config.h (cung thu muc)
 *   2. Dien gia tri THUC TE vao cac o YOUR_...
 *
 * Node nay KHONG tu ket noi Blynk -- chi noi voi AI Server qua HTTP
 * thuan. Moi hien thi/dieu khien tren app Blynk deu do serverFolder
 * dam nhiem (V1..V6 la widget CO DINH cua he thong, khong danh rieng
 * pin nao cho node nay).
 */

//  WiFi 
#define WIFI_SSID     "YOUR_WIFI_SSID"      // Ten mang WiFi
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"  // Mat khau WiFi

//  AI Server 
// Doi thanh IP LAN THUC TE cua may dang chay serverFolder/main.py
// Xem IP bang lenh: ipconfig (Windows) hoac ip addr (Linux/Mac)
#define SERVER_HOST "YOUR_SERVER_LAN_IP"  // VD: "192.168.1.100"
#define SERVER_PORT 8000
#define SENSOR_UPDATE_PATH "/sensor-update"
#define SENSOR_ERROR_PATH  "/sensor-error"
#define SYSTEM_STATUS_PATH "/system-status"

//  Chu ky do & gui du lieu 
// Khi TEST co the doi thanh 5000UL (5 giay) de thay ket qua nhanh.
#define READ_INTERVAL_MS 300000UL   // 900000UL = 15 phut khi trien khai that
