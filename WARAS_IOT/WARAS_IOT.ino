#include <WiFi.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <ModbusMaster.h>
#include <time.h>
#include <ESP32Servo.h>

// --- LIBRARY KHUSUS OTA ---
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// --- PANGGIL FILE FUZZY LEARNING AUTOMATA ---
#include "FuzzyControl.h" 

// --- KONFIGURASI WIFI & FIREBASE ---
#define FIREBASE_HOST "https://waras-iot-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "UkWDgNUkux87bEuWyWcqF6Q9voKwTLck3YCL9n8G"

// --- PIN DEFINITIONS ---
#define RXD2 16 
#define TXD2 17 
#define PH_SENSOR_PIN 34
#define DINAMO_RELAY_PIN 27  
#define FEEDER_SERVO_PIN 12  

// --- VARIABLES SENSOR & WIFI ---
float calibration_value = 21.34;
int buffer_arr[10], temp;
unsigned long int avgval;
unsigned long lastHistorySave = 0;
const unsigned long historyInterval = 60000; 
unsigned long lastWiFiCheck = 0; // Untuk auto-reconnect

// --- VARIABLES AUTO MODE (STATE MACHINE) ---
Servo feederServo;
bool autoModeActive = false;
unsigned long startTime = 0;
int stepAuto = 0;
int durasiBukaServo = 0;

// --- VARIABLES TRIGGER CERDAS ---
unsigned long lastFeedTime = 0;
unsigned long feedCooldown = 300000; // Pengujian 5 menit

// --- OBJECTS ---
ModbusMaster node;
FirebaseData firebaseData;
FirebaseData firebaseControl;
FirebaseConfig config;
FirebaseAuth auth;

// ======================================================================
// FUNGSI EKSEKUTOR OTA (UPDATE JARAK JAUH)
// ======================================================================
void eksekusiOTA(String firmwareUrl) {
  Serial.println("=====================================");
  Serial.println("🚀 MEMULAI UPDATE OTA DARI CLOUD...");
  
  WiFiClientSecure client;
  client.setInsecure(); // Bypass SSL HTTPS

  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("❌ OTA Gagal Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("⚠️ Tidak ada update OTA.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("✅ OTA BERHASIL! ESP32 Restart...");
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // --- STABILITAS WIFI MODEM HI-FI ---
  WiFi.setSleep(false);        
  WiFi.setAutoReconnect(true); 
  WiFi.persistent(true);       

  pinMode(DINAMO_RELAY_PIN, OUTPUT);
  digitalWrite(DINAMO_RELAY_PIN, HIGH); 

  feederServo.attach(FEEDER_SERVO_PIN);
  feederServo.write(48); 

  Serial2.begin(4800, SERIAL_8N1, RXD2, TXD2);
  delay(2000);
  node.begin(1, Serial2);

  WiFiManager wm;
  bool res = wm.autoConnect("WARAS-Setup");
  if(!res) {
    ESP.restart();
  } 

  configTime(25200, 0, "pool.ntp.org", "time.nist.gov");
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("WARAS IoT System Ready, Tuan Muda!");
}

void loop() {
  // --- 0. AUTO RECONNECT WIFI ---
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiCheck >= 5000) {
      WiFi.reconnect();
      lastWiFiCheck = millis();
    }
    return; 
  }

  // --- A. PEMBACAAN SENSOR ---
  float doConcentration = readModbusFloat(0x0002);
  float temperature = readModbusFloat(0x0004);
  float phValue = readPH();
  
  if (doConcentration < -900) doConcentration = 0;
  if (temperature < -900) temperature = 0;

  // --- B. KIRIM DATA KE FIREBASE ---
  FirebaseJson currentData;
  currentData.set("do", doConcentration);
  currentData.set("ph", phValue);
  currentData.set("temperature", temperature);
  currentData.set("timestamp/.sv", "timestamp"); 
  Firebase.setJSON(firebaseData, "/sensors/current", currentData);

  if (millis() - lastHistorySave >= historyInterval) {
    String historyPath = "/sensors/history/" + getYearMonth();
    if (Firebase.pushJSON(firebaseData, historyPath, currentData)) {
      lastHistorySave = millis();
    }
  }

  // --- C. TERIMA KONTROL & OTA ---
  if (Firebase.getString(firebaseControl, "/control/mode")) {
    String mode = firebaseControl.stringData();

    // 🚨 LOGIKA PEMICU OTA 🚨
    if (Firebase.getBool(firebaseControl, "/control/actuators/trigger_ota")) {
      if (firebaseControl.boolData() == true) {
        if (Firebase.getString(firebaseControl, "/firmware_url")) {
          String url = firebaseControl.stringData();
          Firebase.setBool(firebaseData, "/control/actuators/trigger_ota", false);
          eksekusiOTA(url); 
        }
      }
    }

    if (mode == "manual") {
      autoModeActive = false;
      if (Firebase.getBool(firebaseControl, "/control/actuators/feeder")) {
        feederServo.write(firebaseControl.boolData() ? 0 : 48);
      }
      if (Firebase.getBool(firebaseControl, "/control/actuators/pelontar")) {
        digitalWrite(DINAMO_RELAY_PIN, firebaseControl.boolData() ? LOW : HIGH);
      }
    } 
    else if (mode == "otomatis" || mode == "auto") {
      if (Firebase.getBool(firebaseControl, "/control/actuators/start_feed")) {
         if (firebaseControl.boolData() == true && !autoModeActive) {
            autoModeActive = true;
            stepAuto = 0;
            startTime = millis();
            durasiBukaServo = 5000; 
            Firebase.setBool(firebaseData, "/control/actuators/start_feed", false);
         }
      }

      unsigned long currentMillis = millis();
      if (!autoModeActive && (lastFeedTime == 0 || (currentMillis - lastFeedTime >= feedCooldown))) {
        float persentasePakan = hitungAksiFLA(phValue, doConcentration, temperature);
        if (persentasePakan <= 0.0) {
          lastFeedTime = currentMillis;
          durasiBukaServo = 0;
        } 
        else {
          durasiBukaServo = (int)((persentasePakan / 100.0) * 5000);
          autoModeActive = true;
          stepAuto = 0;
          startTime = currentMillis;
          lastFeedTime = currentMillis; 
        }

        // Log Keputusan FLA ke Firebase
        FirebaseJson flaLog;
        flaLog.set("ph", phValue);
        flaLog.set("do", doConcentration);
        flaLog.set("temperature", temperature);
        flaLog.set("fuzzy_rate", persentasePakan);
        flaLog.set("timestamp/.sv", "timestamp");
        String historyPath = "/sensors/history/" + getYearMonth();
        Firebase.pushJSON(firebaseData, historyPath, flaLog);
      }
    }
  }

  // --- D. STATE MACHINE MEKANIK ---
  if (autoModeActive) {
    unsigned long now = millis();
    unsigned long elapsedTime = now - startTime;
    switch (stepAuto) {
      case 0:
        feederServo.write(0); 
        startTime = now;
        stepAuto = 1;
        break;
      case 1:
        if (elapsedTime >= durasiBukaServo) {
          feederServo.write(48); 
          digitalWrite(DINAMO_RELAY_PIN, LOW); 
          startTime = now;
          stepAuto = 2;
        }
        break;
      case 2:
        if (elapsedTime >= 2000) {
          digitalWrite(DINAMO_RELAY_PIN, HIGH); 
          autoModeActive = false;
        }
        break;
    }
  }
  delay(1000);
}

// --- FUNGSI PENDUKUNG ---
String getYearMonth() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "2026-05"; 
  char buf[10];
  strftime(buf, sizeof(buf), "%Y-%m", &timeinfo);
  return String(buf);
}

float readModbusFloat(uint16_t reg) {
  uint8_t result = node.readHoldingRegisters(reg, 2);
  if (result == node.ku8MBSuccess) {
    uint16_t h = node.getResponseBuffer(0);
    uint16_t l = node.getResponseBuffer(1);
    union { uint32_t i; float f; } conv;
    conv.i = ((uint32_t)h << 16) | l;
    return conv.f;
  }
  return -999.9;
}

float readPH() {
  for (int i = 0; i < 10; i++) { buffer_arr[i] = analogRead(PH_SENSOR_PIN); delay(10); }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) { temp = buffer_arr[i]; buffer_arr[i] = buffer_arr[j]; buffer_arr[j] = temp; }
    }
  }
  avgval = 0;
  for (int i = 2; i < 8; i++) avgval += buffer_arr[i];
  if (avgval < 150) return 0.00;
  float voltage = (float)avgval * 3.3 / 4095.0 / 6;
  float ph = -5.70 * voltage + calibration_value;
  return (ph < 0) ? 0 : (ph > 14) ? 14 : ph;
}