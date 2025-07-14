// === Konfigurasi Blynk dan WiFi ===
#define BLYNK_TEMPLATE_ID "TMPL6ys9cZM90"
#define BLYNK_TEMPLATE_NAME "Saklar Wifi"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_TOKEN" // <-- Ganti dengan token Anda

#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>

// === Kredensial WiFi ===
const char ssid[] = "YOUR_SSID";         // <-- Ganti dengan SSID WiFi Anda
const char pass[] = "YOUR_PASSWORD";     // <-- Ganti dengan password WiFi Anda

// === Pin Setup ===
#define RELAY1 D0    // Lampu Depan
#define RELAY2 D7    // Lampu Tengah
#define BUTTON1 D5   // Tombol untuk RELAY1
#define BUTTON2 D6   // Tombol untuk RELAY2
#define LED_WIFI D4  // Indikator WiFi (LED_BUILTIN pada ESP8266)
// (LED aktif LOW pada D4/LED_BUILTIN)

// === Status & Variabel ===
volatile bool relay1On = false;
volatile bool relay2On = false;
volatile bool relay1Manual = false;
volatile bool relay2Manual = false;
bool lastButton1State = HIGH;
bool lastButton2State = HIGH;

unsigned long lastButton1Debounce = 0;
unsigned long lastButton2Debounce = 0;
const unsigned long debounceDelay = 50;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7
RTC_DS3231 rtc;

unsigned long lastTimeCheck = 0;
unsigned long lastRelay1Off = 0;
unsigned long lastRelay2Off = 0;
unsigned long lastDebugPrint = 0;
const unsigned long TIME_CHECK_INTERVAL = 10000;
const unsigned long RELAY_OFF_INTERVAL = 60000;
const unsigned long DEBUG_INTERVAL = 60000;

// Untuk auto-reconnect WiFi
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;

// Untuk sinkronisasi RTC
unsigned long lastRTCSync = 0;
const unsigned long RTC_SYNC_INTERVAL = 6UL * 3600UL * 1000UL; // 6 jam

// === Fungsi Bantuan ===

// Indikator LED WiFi
void updateWiFiLED() {
  digitalWrite(LED_WIFI, WiFi.status() == WL_CONNECTED ? LOW : HIGH);
}

// Koneksi WiFi dengan auto-reconnect
void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(250);
      Serial.print(".");
      attempts++;
      updateWiFiLED();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
    } else {
      Serial.println("\nWiFi reconnect failed.");
    }
  }
  updateWiFiLED();
}

// Sinkronisasi RTC jika selisih waktu > 10 detik atau setiap interval tertentu
void syncRTCwithNTP() {
  if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
    DateTime ntpTime = DateTime(timeClient.getEpochTime());
    DateTime rtcTime = rtc.now();
    long diff = abs(ntpTime.unixtime() - rtcTime.unixtime());
    if (diff > 10 || (millis() - lastRTCSync > RTC_SYNC_INTERVAL)) {
      rtc.adjust(ntpTime);
      Serial.println("RTC synced with NTP.");
      lastRTCSync = millis();
    }
  }
}

// Fungsi penjadwalan auto ON/OFF
void handleScheduler(int hour, int minute) {
  // Relay 1: ON jam >= 17:30, OFF jam >= 23:00
  if (!relay1On && !relay1Manual && (hour > 17 || (hour == 17 && minute >= 30))) {
    digitalWrite(RELAY1, LOW);
    relay1On = true;
    Serial.println("Relay 1 ON (Auto Sched)");
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, 1);
  }
  if (relay1On && (hour > 23 || (hour == 23 && minute >= 0)) && millis() - lastRelay1Off > RELAY_OFF_INTERVAL) {
    digitalWrite(RELAY1, HIGH);
    relay1On = false;
    relay1Manual = false;
    Serial.println("Relay 1 OFF (Auto Sched)");
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, 0);
    lastRelay1Off = millis();
  }
  // Relay 2: ON jam >= 17:30, OFF jam >= 05:30
  if (!relay2On && !relay2Manual && (hour > 17 || (hour == 17 && minute >= 30))) {
    digitalWrite(RELAY2, LOW);
    relay2On = true;
    Serial.println("Relay 2 ON (Auto Sched)");
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, 1);
  }
  if (relay2On && ((hour > 5) || (hour == 5 && minute >= 30)) && millis() - lastRelay2Off > RELAY_OFF_INTERVAL) {
    digitalWrite(RELAY2, HIGH);
    relay2On = false;
    relay2Manual = false;
    Serial.println("Relay 2 OFF (Auto Sched)");
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, 0);
    lastRelay2Off = millis();
  }
}

// Debounce dan handle tombol
void handleButtons() {
  bool button1State = digitalRead(BUTTON1);
  bool button2State = digitalRead(BUTTON2);

  if (button1State != lastButton1State) lastButton1Debounce = millis();
  if (button2State != lastButton2State) lastButton2Debounce = millis();

  if ((millis() - lastButton1Debounce) > debounceDelay) {
    if (button1State == LOW && lastButton1State == HIGH) {
      // Toggle relay 1
      relay1On = !relay1On;
      relay1Manual = relay1On;
      digitalWrite(RELAY1, relay1On ? LOW : HIGH);
      Serial.println(relay1On ? "Relay 1 ON (Button)" : "Relay 1 OFF (Button)");
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, relay1On ? 1 : 0);
    }
  }

  if ((millis() - lastButton2Debounce) > debounceDelay) {
    if (button2State == LOW && lastButton2State == HIGH) {
      // Toggle relay 2
      relay2On = !relay2On;
      relay2Manual = relay2On;
      digitalWrite(RELAY2, relay2On ? LOW : HIGH);
      Serial.println(relay2On ? "Relay 2 ON (Button)" : "Relay 2 OFF (Button)");
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, relay2On ? 1 : 0);
    }
  }

  lastButton1State = button1State;
  lastButton2State = button2State;
}

// === Handler Blynk ===
BLYNK_WRITE(V1) {
  int val = param.asInt();
  relay1On = (val == 1);
  relay1Manual = relay1On;
  digitalWrite(RELAY1, relay1On ? LOW : HIGH);
  Serial.println(relay1On ? "Relay 1 ON (Blynk)" : "Relay 1 OFF (Blynk)");
}

BLYNK_WRITE(V2) {
  int val = param.asInt();
  relay2On = (val == 1);
  relay2Manual = relay2On;
  digitalWrite(RELAY2, relay2On ? LOW : HIGH);
  Serial.println(relay2On ? "Relay 2 ON (Blynk)" : "Relay 2 OFF (Blynk)");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Awal relay OFF
  digitalWrite(RELAY2, HIGH);
  updateWiFiLED();

  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
    while (1);
  }

  // Mulai koneksi WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi ..");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
    updateWiFiLED();
  }
  updateWiFiLED();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    timeClient.begin();
    timeClient.update();
    syncRTCwithNTP();
  } else {
    Serial.println("\nWiFi connection failed. Blynk dan NTP tidak dijalankan.");
  }

  // Setup selesai
  Serial.println("Setup selesai.");
}

void loop() {
  // Watchdog Timer opsional (aktifkan jika perlu pada board support)
  // ESP.wdtFeed();

  // Blynk
  if (WiFi.status() == WL_CONNECTED) Blynk.run();

  // Periksa koneksi WiFi berkala
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    ensureWiFi();
    lastWiFiCheck = millis();
  }

  // Tombol
  handleButtons();

  // Penjadwalan dan waktu
  if (millis() - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    int hour, minute;

    if (WiFi.status() == WL_CONNECTED && timeClient.update()) {
      syncRTCwithNTP();
      hour = timeClient.getHours();
      minute = timeClient.getMinutes();
    } else {
      DateTime now = rtc.now();
      hour = now.hour();
      minute = now.minute();
      Serial.println("Menggunakan RTC (WiFi OFF)");
    }

    // Debug waktu
    if (millis() - lastDebugPrint > DEBUG_INTERVAL) {
      Serial.printf("Waktu: %02d:%02d\n", hour, minute);
      lastDebugPrint = millis();
    }

    handleScheduler(hour, minute);

    lastTimeCheck = millis();
  }
}
