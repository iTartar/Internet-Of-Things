# Panduan Praktikum IoT: Monitoring Suhu & Kelembapan dengan ESP32, DHT22, dan ThingsBoard (via MQTT)

Repository ini berisi panduan dan kode program untuk simulasi monitoring suhu dan kelembapan secara *real-time* menggunakan **ESP32** dan sensor **DHT22** di platform **Wokwi**, yang kemudian datanya dikirim ke IoT Cloud **ThingsBoard** menggunakan protokol **MQTT** murni via `PubSubClient`.

---

## 📁 Struktur Folder Repository

* **/DHT22** : Berisi kode program untuk pengujian sensor DHT22 secara lokal (Serial Monitor).
* **/ThingsBoard** : Berisi kode program untuk integrasi dan pengiriman data ke platform ThingsBoard Cloud via MQTT.

---

## 🛠️ Komponen & Perangkat Lunak
1. **Simulator**: [Wokwi Simulator](https://wokwi.com/)
2. **IoT Platform**: [ThingsBoard Cloud](https://thingsboard.cloud/) (Gunakan region **EU** sesuai konfigurasi kode)
3. **Mikrokontroler**: ESP32
4. **Sensor**: DHT22 (Pin Data disambungkan ke **GPIO 15**)

---

## 📚 Library yang Dibutuhkan (Wokwi Library Manager)
Pastikan kamu sudah menambahkan library berikut pada tab **Library Manager** di Wokwi sebelum menjalankan program:
* `DHT sensor library for ESPx`
* `PubSubClient`

---

## 🚀 Langkah Pengujian

### 1. Tahap Lokal (Folder: `/DHT22`)
Tahap awal ini digunakan untuk memastikan bahwa mikrokontroler ESP32 dapat membaca data dari sensor DHT22 dengan benar secara lokal sebelum dikirim ke internet.

1. Buka [Wokwi](https://wokwi.com/) dan buat project baru dengan board **ESP32**.
2. Rangkai komponen DHT22 ke ESP32 dengan konfigurasi pin:
   * **VCC** -> Pin **3V3**
   * **SDA / Data** -> Pin **D15**
   * **GND** -> Pin **GND**
3. Salin kode program yang ada di dalam folder `DHT22` ke file `sketch.ino` di Wokwi.
4. Jalankan simulasi dengan menekan tombol **Start Simulation** (ikon play hijau).
5. Klik pada komponen DHT22 di layar simulasi untuk mengubah nilai suhu dan kelembapan secara manual.
6. Perhatikan **Serial Monitor** di bagian bawah; jika angka suhu dan kelembapan berubah sesuai dengan yang digeser, maka pengujian lokal **Berhasil**.

---

### 2. Tahap Cloud Integrasi (Folder: `/ThingsBoard`)
Setelah pengujian lokal berhasil, tahap ini akan menghubungkan rangkaian simulasi ke server ThingsBoard agar data bisa dipantau dari jarak jauh melalui grafik (*dashboard*).

#### A. Konfigurasi di ThingsBoard Cloud
1. Masuk ke akun [ThingsBoard Cloud](https://eu.thingsboard.cloud/).
2. Buka menu **Device Groups** -> Klik **All**.
3. Tambahkan perangkat baru dengan klik ikon **(+) Add Device**.
4. Beri nama perangkat (misal: `ESP32-DHT22`) dan klik **Add**.
5. Buka perangkat yang baru dibuat, lalu klik tombol **Copy access token** (Token ini yang akan dimasukkan ke dalam kode).
6. Masuk ke **Device Groups**, pilih grup perangkat kamu, lalu klik **Make entity group public** agar data bisa diakses oleh visualisasi dashboard.

#### B. Setup Kode Program di Wokwi
1. Salin kode program lengkap yang berada di dalam folder `ThingsBoard` ke file `sketch.ino` Wokwi.
2. Cari baris kode berikut dan ganti dengan token yang sudah kamu copy tadi:
   ```cpp
   const char* accessToken = "MASUKKAN_TOKEN_THINGSBOARD_KAMU_DISINI";