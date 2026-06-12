#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ===================== KONFIGURASI PIN =====================
#define DHT_PIN       4
#define SWITCH_PIN    15
#define BUZZER_PIN    5
#define LED_PIN       2   // LED merah = indikator kipas aktif

// ===================== KONFIGURASI DHT =====================
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// ===================== KONFIGURASI WIFI =====================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ===================== KONFIGURASI MQTT =====================
const char* MQTT_BROKER   = "broker.hivemq.com"; // ganti jika pakai Mosquitto lokal
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT   = "cold_storage_36";
const char* DEVICE_ID     = "NRP36";

// Topic MQTT (personalisasi NRP 36)
const char* TOPIC_TELEMETRY     = "iot/kelas/36/telemetry";
const char* TOPIC_STATUS        = "iot/kelas/36/status";
const char* TOPIC_ALARM         = "iot/kelas/36/alarm";
const char* TOPIC_KIPAS_CMD     = "iot/kelas/36/aktuator/kipas";
const char* TOPIC_BUZZER_CMD    = "iot/kelas/36/aktuator/buzzer";

// ===================== PARAMETER NRP 36 =====================
const float SUHU_WASPADA        = 34.0;  // 28 + digit terakhir NRP (6)
const float SUHU_NORMAL         = 28.0;
const float SUHU_RULE_KHUSUS    = 30.0;  // rule khusus NRP 36
const float KELEMBABAN_TINGGI   = 80.0;  // rule khusus: kombinasi suhu+kelembaban
const float SUHU_MIN_VALID      = -50.0; // batas validasi sensor
const float SUHU_MAX_VALID      = 85.0;
const int   INTERVAL_KIRIM      = 5000;  // 5 detik (2 + digit kedua terakhir NRP = 3)
const int   DOOR_OPEN_LIMIT     = 30000; // 30 detik pintu terbuka = bahaya

// ===================== STATE SISTEM =====================
bool kipasAktif   = false;
bool buzzerAktif  = false;
bool manualKipas  = false;
bool manualBuzzer = false;
unsigned long doorOpenSince   = 0;
unsigned long lastKirimData   = 0;
unsigned long lastReconnect   = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ===================== FUNGSI KONEKSI WIFI =====================
void connectWifi() {
  Serial.print("[WiFi] Menghubungkan ke ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Terhubung! IP: " + WiFi.localIP().toString());
}

// ===================== CALLBACK MQTT (TERIMA PERINTAH) =====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String pesan = "";
  for (int i = 0; i < length; i++) pesan += (char)payload[i];
  pesan.trim();

  Serial.println("[MQTT] Pesan masuk - Topic: " + String(topic) + " | Pesan: " + pesan);

  // Kendali manual kipas dari dashboard
  if (String(topic) == TOPIC_KIPAS_CMD) {
    if (pesan == "ON") {
      manualKipas = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("[AKTUATOR] Kipas ON (manual)");
    } else if (pesan == "OFF") {
      manualKipas = false;
      // LED akan dikontrol rule otomatis di loop berikutnya
      Serial.println("[AKTUATOR] Kipas OFF (manual)");
    }
  }

  // Kendali manual buzzer dari dashboard
  if (String(topic) == TOPIC_BUZZER_CMD) {
    if (pesan == "ON") {
      manualBuzzer = true;
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("[AKTUATOR] Buzzer ON (manual)");
    } else if (pesan == "OFF") {
      manualBuzzer = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("[AKTUATOR] Buzzer OFF (manual)");
    }
  }
}

// ===================== KONEKSI & RECONNECT MQTT =====================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Menghubungkan...");
    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println(" Terhubung!");
      mqtt.subscribe(TOPIC_KIPAS_CMD);
      mqtt.subscribe(TOPIC_BUZZER_CMD);
      // Kirim status online
      mqtt.publish(TOPIC_STATUS, "{\"status\":\"ONLINE\",\"device\":\"NRP36\"}");
    } else {
      Serial.print(" Gagal, rc=");
      Serial.print(mqtt.state());
      Serial.println(" | Coba lagi 5 detik...");
      delay(5000);
    }
  }
}

// ===================== VALIDASI SENSOR =====================
bool sensorValid(float suhu, float kelembaban) {
  if (isnan(suhu) || isnan(kelembaban)) return false;
  if (suhu < SUHU_MIN_VALID || suhu > SUHU_MAX_VALID) return false;
  if (kelembaban < 0 || kelembaban > 100) return false;
  return true;
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  Serial.println("=== Cold Storage Monitor - NRP 36 ===");

  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();
  connectWifi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  connectMQTT();
}

// ===================== LOOP UTAMA =====================
void loop() {
  // Reconnect otomatis jika MQTT terputus
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnect > 5000) {
      lastReconnect = now;
      Serial.println("[MQTT] Koneksi terputus, mencoba reconnect...");
      connectMQTT();
    }
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastKirimData < INTERVAL_KIRIM) return;
  lastKirimData = now;

  // ===== BACA SENSOR =====
  float suhu       = dht.readTemperature();
  float kelembaban = dht.readHumidity();
  bool  pintTerbuka = (digitalRead(SWITCH_PIN) == LOW); // LOW = ditekan = pintu terbuka

  // ===== VALIDASI SENSOR =====
  if (!sensorValid(suhu, kelembaban)) {
    Serial.println("[ERROR] Nilai sensor tidak valid!");
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"SENSOR_ERROR\",\"device\":\"NRP36\"}");
    mqtt.publish(TOPIC_ALARM,  "{\"alarm\":true,\"pesan\":\"Sensor error - data tidak valid\"}");
    return;
  }

  // ===== TRACKING PINTU TERBUKA =====
  if (pintTerbuka) {
    if (doorOpenSince == 0) doorOpenSince = now;
  } else {
    doorOpenSince = 0;
  }
  bool doorTerlalLama = (doorOpenSince > 0 && (now - doorOpenSince > DOOR_OPEN_LIMIT));

  // ===== RULE OTOMATIS =====
  String statusSistem = "NORMAL";
  bool  alarmAktif   = false;
  String pesanAlarm  = "";

  // Rule 1: Suhu > 34°C → kipas menyala
  if (suhu > SUHU_WASPADA) {
    kipasAktif   = true;
    statusSistem = "WASPADA";
  }

  // Rule 2: Suhu <= 28°C → kipas mati (hanya jika tidak manual)
  if (suhu <= SUHU_NORMAL && !manualKipas) {
    kipasAktif = false;
  }

  // Rule 3: Pintu terbuka > 30 detik → buzzer + alarm BAHAYA
  if (doorTerlalLama) {
    buzzerAktif  = true;
    statusSistem = "BAHAYA";
    alarmAktif   = true;
    pesanAlarm   = "Pintu terbuka lebih dari 30 detik!";
  }

  // Rule 4: Suhu > 34°C DAN pintu terbuka → KRITIS
  if (suhu > SUHU_WASPADA && pintTerbuka) {
    kipasAktif   = true;
    buzzerAktif  = true;
    statusSistem = "BAHAYA";
    alarmAktif   = true;
    pesanAlarm   = "KRITIS: Suhu tinggi + pintu terbuka!";
  }

  // Rule 5 (Rule Khusus NRP 36): Suhu > 30°C DAN kelembaban > 80%
  if (suhu > SUHU_RULE_KHUSUS && kelembaban > KELEMBABAN_TINGGI) {
    kipasAktif   = true;
    if (statusSistem == "NORMAL") statusSistem = "WASPADA";
    alarmAktif   = true;
    pesanAlarm   = "Kombinasi suhu tinggi + kelembaban tinggi!";
  }

  // Kembali normal: suhu aman + pintu tertutup + tidak ada kondisi bahaya
  if (suhu <= SUHU_NORMAL && !pintTerbuka && !doorTerlalLama) {
    if (!manualBuzzer) buzzerAktif = false;
    if (!manualKipas)  kipasAktif  = false;
    statusSistem = "NORMAL";
    alarmAktif   = false;
  }

  // ===== KONTROL AKTUATOR =====
  digitalWrite(LED_PIN,    (kipasAktif  || manualKipas)  ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, (buzzerAktif || manualBuzzer) ? HIGH : LOW);

  // ===== KIRIM TELEMETRY =====
  StaticJsonDocument<256> doc;
  doc["device_id"]       = DEVICE_ID;
  doc["timestamp"]       = now;
  doc["suhu"]            = suhu;
  doc["kelembaban"]      = kelembaban;
  doc["pintu_terbuka"]   = pintTerbuka;
  doc["kipas_aktif"]     = kipasAktif || manualKipas;
  doc["buzzer_aktif"]    = buzzerAktif || manualBuzzer;
  doc["status"]          = statusSistem;

  char buffer[256];
  serializeJson(doc, buffer);
  mqtt.publish(TOPIC_TELEMETRY, buffer);

  // ===== KIRIM STATUS =====
  StaticJsonDocument<128> docStatus;
  docStatus["device_id"] = DEVICE_ID;
  docStatus["status"]    = statusSistem;
  docStatus["timestamp"] = now;
  char bufStatus[128];
  serializeJson(docStatus, bufStatus);
  mqtt.publish(TOPIC_STATUS, bufStatus);

  // ===== KIRIM ALARM (jika ada) =====
  if (alarmAktif && pesanAlarm != "") {
    StaticJsonDocument<128> docAlarm;
    docAlarm["alarm"]     = true;
    docAlarm["pesan"]     = pesanAlarm;
    docAlarm["timestamp"] = now;
    char bufAlarm[128];
    serializeJson(docAlarm, bufAlarm);
    mqtt.publish(TOPIC_ALARM, bufAlarm);
  }

  // ===== LOG SERIAL MONITOR =====
  Serial.println("----------------------------------------");
  Serial.printf("[DATA] Suhu: %.1f°C | Kelembaban: %.1f%%\n", suhu, kelembaban);
  Serial.printf("[DATA] Pintu: %s | Kipas: %s | Buzzer: %s\n",
    pintTerbuka ? "TERBUKA" : "TERTUTUP",
    (kipasAktif || manualKipas) ? "ON" : "OFF",
    (buzzerAktif || manualBuzzer) ? "ON" : "OFF"
  );
  Serial.printf("[STATUS] %s\n", statusSistem.c_str());
  if (alarmAktif) Serial.printf("[ALARM] %s\n", pesanAlarm.c_str());
}
