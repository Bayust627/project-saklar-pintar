// ==== BLYNK IoT SETUP ====
#define BLYNK_TEMPLATE_ID "TMPL6ys9cZM90"
#define BLYNK_TEMPLATE_NAME "Saklar Wifi"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ==== CREDENTIAL ====
char auth[] = "fjVwVO98MYQwpivprw8cfOTH1msKxnNv"; // Harus diubah di aplikasi
char ssid[] = "SESEK BALAP"; // Harus diubah di aplikasi
char pass[] = "ojoraoleh"; // Harus diubah di aplikasi

// ==== PIN SETTING ====
#define RELAY1     16 // GPIO16 (D0)
#define RELAY2     13 // GPIO13 (D7)
#define BUTTON1    14 // GPIO14 (D5)
#define BUTTON2    12 // GPIO12 (D6)

// ==== STATUS ====
bool relay1Status = false;
bool relay2Status = false;
bool relay1Manual = false;
bool relay2Manual = false;

bool lastButton1State = HIGH;
bool lastButton2State = HIGH;

// ==== WAKTU & TIMER ====
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7 (WIB)

RTC_DS3231 rtc;

unsigned long lastTimeCheck = 0;
unsigned long lastRelay1Off = 0;
unsigned long lastRelay2Off = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastReconnect = 0;
unsigned long lastNTPSync = 0;
unsigned long lastEventSent = 0; // Untuk debounce event Blynk

const unsigned long TIME_CHECK_INTERVAL = 1000; // Pengecekan jadwal setiap 1 detik
const unsigned long RELAY_OFF_INTERVAL = 60000; // Jeda untuk mencegah switching cepat
const unsigned long BUTTON_DEBOUNCE_INTERVAL = 10; // Interval pembacaan tombol (ms)
const unsigned long NTP_SYNC_INTERVAL = 3600000; // Sinkron NTP setiap 1 jam (untuk akurasi)
const unsigned long RECONNECT_INTERVAL = 5000; // Interval rekoneksi WiFi/Blynk (5 detik)
const unsigned long EVENT_DEBOUNCE_INTERVAL = 1000; // Minimal 1 detik antar event

// ==== JADWAL DINAMIS ====
int relay1OnHour = 17, relay1OnMinute = 30, relay1OffHour = 23, relay1OffMinute = 0;
int relay2OnHour = 17, relay2OnMinute = 30, relay2OffHour = 5, relay2OffMinute = 30;

// ==== FUNGSI UNTUK MENGIRIM EVENT BLYNK DENGAN DEBOUNCE ====
void sendBlynkEvent(const char* eventCode, const char* message) {
  if (millis() - lastEventSent >= EVENT_DEBOUNCE_INTERVAL && WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.logEvent(eventCode, message);
    lastEventSent = millis();
    Serial.print("Event sent [");
    Serial.print(eventCode);
    Serial.print("]: ");
    Serial.println(message);
  } else {
    Serial.print("Event skipped [");
    Serial.print(eventCode);
    Serial.print("]: ");
    Serial.println(message);
  }
}

// ==== SYNC NTP KE RTC ====
void syncTimeToRTC() {
  if (WiFi.status() == WL_CONNECTED && ntpClient.update()) {
    DateTime ntpTime(ntpClient.getEpochTime());
    DateTime rtcTime = rtc.now();
    long timeDiff = (long)(ntpTime.unixtime() - rtcTime.unixtime());
    if (rtcTime.unixtime() < 946684800 || abs(timeDiff) > 60) { // Sinkron jika selisih > 1 menit
      rtc.adjust(ntpTime);
      Serial.print("RTC synced with NTP, diff: ");
      Serial.println(timeDiff);
      sendBlynkEvent("rtc_sync", "RTC synced with NTP");
    } else {
      Serial.print("RTC in sync, diff: ");
      Serial.println(timeDiff);
    }
  }
}

// ==== KONTROL RELAY ====
void setRelay(uint8_t relayPin, bool state, bool isRelay1 = true) {
  static unsigned long lastRelay1Change = 0, lastRelay2Change = 0;
  unsigned long currentTime = millis();
  
  if (isRelay1 && currentTime - lastRelay1Change < 1000) return; // Jeda 1 detik untuk Relay 1
  if (!isRelay1 && currentTime - lastRelay2Change < 1000) return; // Jeda 1 detik untuk Relay 2

  digitalWrite(relayPin, state ? LOW : HIGH); // LOW = ON
  if (isRelay1) {
    relay1Status = state;
    lastRelay1Change = currentTime;
  } else {
    relay2Status = state;
    lastRelay2Change = currentTime;
  }

  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.virtualWrite(isRelay1 ? V1 : V2, state ? 1 : 0);
    char msg[50];
    snprintf(msg, sizeof(msg), "Relay %d %s", isRelay1 ? 1 : 2, state ? "ON" : "OFF");
    sendBlynkEvent(isRelay1 ? "relay1_status" : "relay2_status", msg);
  } else {
    Serial.print("Relay status updated locally, V");
    Serial.print(isRelay1 ? "1" : "2");
    Serial.print(": ");
    Serial.println(state ? "1" : "0");
  }
}

// ==== HANDLE PUSH BUTTON ====
void checkButtons() {
  static int button1Count = 0, button2Count = 0;

  if (millis() - lastButtonCheck < BUTTON_DEBOUNCE_INTERVAL) return;

  bool button1 = digitalRead(BUTTON1);
  bool button2 = digitalRead(BUTTON2);

  // Debounce untuk tombol 1
  if (button1 == lastButton1State) {
    button1Count = 0; // Reset hitungan jika status tidak berubah
  } else {
    button1Count++;
    if (button1Count >= 5) { // Membutuhkan 5 pembacaan stabil
      if (button1 == LOW && lastButton1State == HIGH) { // Transisi dari HIGH ke LOW
        relay1Status = !relay1Status;
        relay1Manual = relay1Status;
        setRelay(RELAY1, relay1Status, true);
        Serial.println(relay1Status ? "Relay 1 ON (Button)" : "Relay 1 OFF (Button)");
      }
      lastButton1State = button1;
      button1Count = 0;
    }
  }

  // Debounce untuk tombol 2
  if (button2 == lastButton2State) {
    button2Count = 0; // Reset hitungan jika status tidak berubah
  } else {
    button2Count++;
    if (button2Count >= 5) { // Membutuhkan 5 pembacaan stabil
      if (button2 == LOW && lastButton2State == HIGH) { // Transisi dari HIGH ke LOW
        relay2Status = !relay2Status;
        relay2Manual = relay2Status;
        setRelay(RELAY2, relay2Status, false);
        Serial.println(relay2Status ? "Relay 2 ON (Button)" : "Relay 2 OFF (Button)");
      }
      lastButton2State = button2;
      button2Count = 0;
    }
  }

  lastButtonCheck = millis();
}

// ==== BLYNK VIRTUAL PIN ====
BLYNK_WRITE(V1) {
  int val = param.asInt();
  relay1Status = (val == 1);
  relay1Manual = relay1Status;
  setRelay(RELAY1, relay1Status, true);
}

BLYNK_WRITE(V2) {
  int val = param.asInt();
  relay2Status = (val == 1);
  relay2Manual = relay2Status;
  setRelay(RELAY2, relay2Status, false);
}

// ==== BLYNK JADWAL DINAMIS ====
BLYNK_WRITE(V3) { relay1OnHour = param.asInt(); }   // Jam ON Relay 1
BLYNK_WRITE(V4) { relay1OnMinute = param.asInt(); } // Menit ON Relay 1
BLYNK_WRITE(V5) { relay1OffHour = param.asInt(); }  // Jam OFF Relay 1
BLYNK_WRITE(V6) { relay1OffMinute = param.asInt();} // Menit OFF Relay 1
BLYNK_WRITE(V7) { relay2OnHour = param.asInt(); }   // Jam ON Relay 2
BLYNK_WRITE(V8) { relay2OnMinute = param.asInt(); } // Menit ON Relay 2
BLYNK_WRITE(V9) { relay2OffHour = param.asInt(); }  // Jam OFF Relay 2
BLYNK_WRITE(V10) { relay2OffMinute = param.asInt();} // Menit OFF Relay 2

// ==== SYNC STATUS SAAT RECONNECT KE BLYNK ====
BLYNK_CONNECTED() {
  Serial.println("Connected to Blynk server, syncing status...");
  Blynk.virtualWrite(V1, relay1Status ? 1 : 0);
  Blynk.virtualWrite(V2, relay2Status ? 1 : 0);
  sendBlynkEvent("blynk_connect", "Device reconnected to Blynk");
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);

  // Inisialisasi pin relay ke OFF untuk mencegah aktif saat boot
  digitalWrite(RELAY1, HIGH); // HIGH = OFF
  digitalWrite(RELAY2, HIGH); // HIGH = OFF
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);

  if (!rtc.begin()) {
    Serial.println("RTC not found! Using NTP only.");
    sendBlynkEvent("rtc_failed", "RTC not found! Using NTP only.");
  } else {
    Serial.println("RTC initialized.");
  }

  WiFi.begin(ssid, pass);
  Blynk.config(auth); // Non-blocking
  ntpClient.begin();
  syncTimeToRTC();
}

// ==== LOOP ====
void loop() {
  Blynk.run();

  // Rekoneksi WiFi dan Blynk
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnect > RECONNECT_INTERVAL) {
    WiFi.begin(ssid, pass);
    lastReconnect = millis();
    Serial.println("Reconnecting to WiFi...");
    sendBlynkEvent("wifi_disconnect", "WiFi disconnected, attempting to reconnect...");
  } else if (WiFi.status() == WL_CONNECTED && !Blynk.connected() && millis() - lastReconnect > RECONNECT_INTERVAL) {
    Blynk.connect();
    lastReconnect = millis();
    Serial.println("Reconnecting to Blynk...");
  }

  // Sinkron NTP berkala
  if (millis() - lastNTPSync > NTP_SYNC_INTERVAL) {
    syncTimeToRTC();
    lastNTPSync = millis();
  }

  checkButtons();

  // Periksa jadwal setiap 1 detik
  if (millis() - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    int hour, minute;

    if (WiFi.status() == WL_CONNECTED && ntpClient.update()) {
      hour = ntpClient.getHours();
      minute = ntpClient.getMinutes();
      Serial.print("NTP Time: ");
      Serial.print(hour);
      Serial.print(":");
      Serial.println(minute);
    } else if (rtc.begin()) {
      DateTime now = rtc.now();
      hour = now.hour();
      minute = now.minute();
      Serial.print("RTC Fallback: ");
      Serial.print(hour);
      Serial.print(":");
      Serial.println(minute);
    } else {
      Serial.println("No time source available!");
      sendBlynkEvent("no_time_source", "No time source available!");
      return;
    }

    // Jadwal Relay 1 dengan jendela waktu ±1 menit
    if (!relay1Status && !relay1Manual && hour == relay1OnHour && minute >= relay1OnMinute - 1 && minute <= relay1OnMinute + 1) {
      relay1Status = true;
      setRelay(RELAY1, true, true);
      Serial.println("Relay 1 ON (Auto)");
    }
    if (relay1Status && hour == relay1OffHour && minute >= relay1OffMinute - 1 && minute <= relay1OffMinute + 1 && millis() - lastRelay1Off >= RELAY_OFF_INTERVAL) {
      relay1Status = false;
      relay1Manual = false;
      setRelay(RELAY1, false, true);
      lastRelay1Off = millis();
      Serial.println("Relay 1 OFF (Auto)");
    }

    // Jadwal Relay 2 dengan jendela waktu ±1 menit
    if (!relay2Status && !relay2Manual && hour == relay2OnHour && minute >= relay2OnMinute - 1 && minute <= relay2OnMinute + 1) {
      relay2Status = true;
      setRelay(RELAY2, true, false);
      Serial.println("Relay 2 ON (Auto)");
    }
    if (relay2Status && hour == relay2OffHour && minute >= relay2OffMinute - 1 && minute <= relay2OffMinute + 1 && millis() - lastRelay2Off >= RELAY_OFF_INTERVAL) {
      relay2Status = false;
      relay2Manual = false;
      setRelay(RELAY2, false, false);
      lastRelay2Off = millis();
      Serial.println("Relay 2 OFF (Auto)");
    }

    lastTimeCheck = millis();
  }
}
