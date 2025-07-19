# 🌐 Saklar Wifi Otomatis Berbasis ESP8266 + Blynk + RTC + NTP

Proyek ini adalah sistem saklar pintar yang dirancang dengan mikrokontroler **ESP8266 (NodeMCU)**. Perangkat ini memungkinkan pengguna mengontrol dua relay secara manual melalui tombol fisik, melalui **aplikasi Blynk**, dan juga secara **otomatis berdasarkan jadwal waktu**. Sistem menggunakan **RTC DS3231** dan **NTP** untuk sinkronisasi waktu, serta mendukung notifikasi status melalui **fitur `Blynk.logEvent()`**.

---

## ✨ Fitur Lengkap

- ✅ **Kontrol Manual** via dua tombol fisik (Push Button)
- ✅ **Kontrol Jarak Jauh** melalui aplikasi **Blynk IoT** (Virtual Pin)
- ✅ **Jadwal Otomatis** berdasarkan jam dan menit untuk ON dan OFF tiap relay
- ✅ **Sinkronisasi Waktu** dari:
  - RTC DS3231 (Offline Mode)
  - NTP Server (Online Mode)
- ✅ **Notifikasi Kejadian (Event)** melalui Blynk dengan logEvent:
  - Relay ON/OFF
  - RTC gagal terdeteksi
  - Sinkronisasi waktu berhasil
  - WiFi disconnect
  - Tidak ada sumber waktu
- ✅ Debounce tombol fisik
- ✅ Interval waktu untuk mencegah switching relay yang terlalu cepat
- ✅ Penjadwalan fleksibel dari aplikasi Blynk melalui Virtual Pin (V3–V10)

---

## 🧰 Komponen yang Dibutuhkan

| Komponen         | Jumlah | Catatan |
|------------------|--------|---------|
| ESP8266 NodeMCU  | 1      | Mikrokontroler utama |
| RTC DS3231       | 1      | Real Time Clock |
| Relay Module 2-Channel | 1 | Mengendalikan beban |
| Push Button      | 2      | Tombol kontrol manual |
| Breadboard & Jumper | secukupnya | Untuk sambungan |
| Power Supply 5V  | 1      | Disarankan min. 1A |
| Koneksi WiFi     | 1      | Untuk kontrol cloud dan NTP |

---

## 📌 Pinout ESP8266 NodeMCU

| Fungsi     | GPIO | NodeMCU Pin |
|------------|------|-------------|
| Relay 1    | 16   | D0          |
| Relay 2    | 13   | D7          |
| Button 1   | 14   | D5          |
| Button 2   | 12   | D6          |
| RTC DS3231 | I2C  | D1 (SCL), D2 (SDA) |

---

## 📷 Wiring Diagram

![Wiring Diagram](wiring.jpg)

Silakan ganti nama file `wiring.jpg` sesuai nama file rangkaian kamu jika berbeda.

---

## 🧠 Logika Program

### 1. **Startup dan Setup**
- Inisialisasi pin, koneksi WiFi, RTC, dan Blynk
- Sinkronisasi waktu dari NTP ke RTC jika tersedia

### 2. **Loop Utama**
- Cek koneksi WiFi
- Jalankan `Blynk.run()`
- Cek tombol fisik setiap 10 ms
- Cek dan jalankan jadwal relay setiap 1 detik
- Sinkronisasi ulang waktu setiap 24 jam

### 3. **Kontrol Manual**
- Tombol fisik dengan debounce
- Status disinkron ke Blynk melalui `virtualWrite` dan `logEvent`

### 4. **Kontrol Otomatis**
- Waktu ON/OFF relay berdasarkan jam & menit (bisa diatur dari aplikasi Blynk)
- Setelah relay dimatikan secara otomatis, flag manual akan di-reset

### 5. **Notifikasi (logEvent)**
Event berikut dikirim ke Blynk:
- `relay1_status`
- `relay2_status`
- `rtc_sync`
- `rtc_failed`
- `wifi_disconnect`
- `no_time_source`

Pastikan event ini telah dibuat di [Blynk Console](https://blynk.cloud/) dan push notification diaktifkan.

---

## 🔢 Virtual Pin Blynk

| Fungsi                | Virtual Pin |
|------------------------|-------------|
| Kontrol Relay 1        | V1          |
| Kontrol Relay 2        | V2          |
| Jam ON Relay 1         | V3          |
| Menit ON Relay 1       | V4          |
| Jam OFF Relay 1        | V5          |
| Menit OFF Relay 1      | V6          |
| Jam ON Relay 2         | V7          |
| Menit ON Relay 2       | V8          |
| Jam OFF Relay 2        | V9          |
| Menit OFF Relay 2      | V10         |

---

## 🚀 Cara Menjalankan

1. Buka kode di Arduino IDE, install library berikut:
   - `Blynk` (versi baru untuk Blynk IoT)
   - `ESP8266WiFi`
   - `RTClib`
   - `NTPClient`

2. Edit bagian kredensial:
```cpp
char auth[] = "Token dari Blynk";
char ssid[] = "Nama WiFi kamu";
char pass[] = "Password WiFi kamu";
```

3. Upload kode ke NodeMCU

4. Di Blynk Console:
   - Buat Template dan Device
   - Tambahkan event sesuai daftar di atas (aktifkan notifikasi)
   - Tambahkan widget Number Input untuk atur jam & menit (V3–V10)

5. Lihat hasilnya di serial monitor dan aplikasi Blynk!

---

## 🧪 Troubleshooting

| Masalah | Solusi |
|--------|--------|
| Notifikasi tidak muncul | Pastikan `logEvent` sesuai nama event di Blynk Console |
| RTC tidak sinkron | Cek koneksi I2C, atau pastikan NTP tersedia pertama kali |
| Tombol tidak responsif | Periksa wiring dan debounce di kode |
| Relay aktif saat startup | Atur `setRelay(..., false)` di `setup()`

---

---

## 👨‍💻 Author

Dibuat oleh **Bay** sebagai proyek pembelajaran IoT menggunakan NodeMCU dan Blynk.  
Support open-source dan semangat belajar bareng!

---

## 📃 License

Proyek ini bebas digunakan untuk pembelajaran dan non-komersial. Jangan lupa cantumkan kredit jika digunakan atau dibagikan ulang.

---

> Made with ❤️ by Bay | 2025
