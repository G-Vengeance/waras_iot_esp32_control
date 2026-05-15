content = """# 🌊 WARAS - Water Quality Monitoring & Auto Feeding System

Dokumentasi teknis untuk sistem Node IoT berbasis ESP32 yang digunakan dalam proyek **WARAS**. Sistem ini melakukan monitoring kualitas air (pH, DO, Suhu) secara real-time dan melakukan pengendalian pemberian pakan otomatis.

---

## 🛠️ Spesifikasi Perangkat Keras (Hardware)
Sistem ini menggunakan mikrokontroler **ESP32** dengan konfigurasi pin sebagai berikut:

| Perangkat | Pin ESP32 | Deskripsi |
| :--- | :--- | :--- |
| **Sensor pH** | GPIO 34 (ADC) | Membaca nilai pH air secara analog |
| **Modbus RX** | GPIO 16 (RX2) | Jalur data masuk dari sensor DO & Suhu (RS485) |
| **Modbus TX** | GPIO 17 (TX2) | Jalur data keluar ke sensor DO & Suhu (RS485) |
| **Relay Feeder** | GPIO 27 | Menggerakkan katup pakan (Active-Low) |
| **Servo Pelontar** | GPIO 12 | Menggerakkan mekanisme penembak pakan |

---

## 💻 Persiapan Perangkat Lunak

### 1. Board Manager ESP32
Pastikan Arduino IDE Anda sudah terinstal board ESP32. Jika belum:
1. Pergi ke **File** > **Preferences**.
2. Masukkan URL berikut ke *Additional Boards Manager URLs*:  
   `https://dl.espressif.com/dl/package_esp32_index.json`
3. Buka **Tools** > **Board** > **Boards Manager**, cari `esp32` dan instal.

### 2. Library yang Dibutuhkan
Unduh library berikut melalui Library Manager (Ctrl+Shift+I) atau link yang disediakan:

| Nama Library | Pembuat | Kegunaan | Link |
| :--- | :--- | :--- | :--- |
| **Firebase ESP32 Client** | Mobizt | Koneksi ke Firebase Realtime Database | [GitHub](https://github.com/mobizt/Firebase-ESP32) |
| **ModbusMaster** | Doc Walker | Komunikasi RS485 dengan sensor industri | [GitHub](https://github.com/4-20ma/ModbusMaster) |
| **ESP32Servo** | Kevin Harrington | Kontrol Motor Servo khusus ESP32 | [GitHub](https://github.com/madhephaestus/ESP32Servo) |

---

## 🚀 Instalasi & Konfigurasi

1. Buka file `.ino` di Arduino IDE.
2. Pastikan **Board** diatur ke `ESP32 Dev Module`.
3. Sesuaikan konfigurasi WiFi dan Firebase pada bagian atas kode:
   ```cpp
   #define WIFI_SSID "Nama_WiFi"
   #define WIFI_PASSWORD "Password_WiFi"
   #define FIREBASE_HOST "URL_Firebase_Tuan_Muda"
   #define FIREBASE_AUTH "Database_Secret_Tuan_Muda"