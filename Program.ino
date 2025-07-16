// ==== BLYNK IoT SETUP ====
#define BLYNK_TEMPLATE_ID "idtemplate"
#define BLYNK_TEMPLATE_NAME "namatemplate"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ==== CREDENTIAL ====
char auth[] = "isitokenblynkkamu";
char ssid[] = "ssid wifi kamu";
char pass[] = "passwordwifikamu";

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
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7

RTC_DS3231 rtc;

unsigned long lastTimeCheck = 0;
unsigned long lastRelay1Off = 0;
unsigned long lastRelay2Off = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastReconnect = 0;

const unsigned long TIME_CHECK_INTERVAL = 10000;
const unsigned long RELAY_OFF_INTERVAL = 60000;
const unsigned long BUTTON_DEBOUNCE_INTERVAL = 50;

// ==== SYNC NTP KE RTC ====
void syncTimeToRTC() {
  if (WiFi.status() == WL_CONNECTED && ntpClient.update()) {
    rtc.adjust(DateTime(ntpClient.getEpochTime()));
    Serial.println("RTC updated from NTP");
  }
}

// ==== KONTROL RELAY ====
void setRelay(uint8_t relayPin, bool state) {
  digitalWrite(relayPin, state ? LOW : HIGH); // LOW = ON
}

void updateRelayStatus() {
  setRelay(RELAY1, relay1Status);
  setRelay(RELAY2, relay2Status);
}

// ==== HANDLE PUSH BUTTON ====
void checkButtons() {
  if (millis() - lastButtonCheck < BUTTON_DEBOUNCE_INTERVAL) return;

  bool button1 = digitalRead(BUTTON1);
  bool button2 = digitalRead(BUTTON2);

  if (button1 == LOW && lastButton1State == HIGH) {
    relay1Status = !relay1Status;
    relay1Manual = relay1Status;
    setRelay(RELAY1, relay1Status);
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, relay1Status ? 1 : 0);
    Serial.println(relay1Status ? "Relay 1 ON (Button)" : "Relay 1 OFF (Button)");
  }

  if (button2 == LOW && lastButton2State == HIGH) {
    relay2Status = !relay2Status;
    relay2Manual = relay2Status;
    setRelay(RELAY2, relay2Status);
    if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, relay2Status ? 1 : 0);
    Serial.println(relay2Status ? "Relay 2 ON (Button)" : "Relay 2 OFF (Button)");
  }

  lastButton1State = button1;
  lastButton2State = button2;
  lastButtonCheck = millis();
}

// ==== BLYNK VIRTUAL PIN ====
BLYNK_WRITE(V1) {
  int val = param.asInt();
  relay1Status = (val == 1);
  relay1Manual = relay1Status;
  setRelay(RELAY1, relay1Status);
}

BLYNK_WRITE(V2) {
  int val = param.asInt();
  relay2Status = (val == 1);
  relay2Manual = relay2Status;
  setRelay(RELAY2, relay2Status);
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);

  setRelay(RELAY1, false);
  setRelay(RELAY2, false);

  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }

  WiFi.begin(ssid, pass);
  Blynk.config(auth);  // Non-blocking

  ntpClient.begin();
  syncTimeToRTC();
}

// ==== LOOP ====
void loop() {
  Blynk.run();

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnect > 10000) {
      WiFi.begin(ssid, pass);
      lastReconnect = millis();
    }
  }

  checkButtons();

  if (millis() - lastTimeCheck >= TIME_CHECK_INTERVAL) {
    int hour, minute;

    if (WiFi.status() == WL_CONNECTED && ntpClient.update()) {
      hour = ntpClient.getHours();
      minute = ntpClient.getMinutes();
    } else {
      DateTime now = rtc.now();
      hour = now.hour();
      minute = now.minute();
      Serial.print("RTC Fallback: ");
      Serial.print(hour); Serial.print(":" ); Serial.println(minute);
    }

    // ==== Jadwal Otomatis Relay 1 ====
    if (!relay1Status && !relay1Manual && hour == 17 && (minute >= 30 && minute <= 31)) {
      relay1Status = true;
      setRelay(RELAY1, true);
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, 1);
      Serial.println("Relay 1 ON (Auto 17:30)");
    }

    if (relay1Status && hour == 23 && minute <= 1 && millis() - lastRelay1Off >= RELAY_OFF_INTERVAL) {
      relay1Status = false;
      relay1Manual = false;
      setRelay(RELAY1, false);
      lastRelay1Off = millis();
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V1, 0);
      Serial.println("Relay 1 OFF (Auto 23:00)");
    }

    // ==== Jadwal Otomatis Relay 2 ====
    if (!relay2Status && !relay2Manual && hour == 17 && (minute >= 30 && minute <= 31)) {
      relay2Status = true;
      setRelay(RELAY2, true);
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, 1);
      Serial.println("Relay 2 ON (Auto 17:30)");
    }

    if (relay2Status && hour == 5 && minute <= 31 && millis() - lastRelay2Off >= RELAY_OFF_INTERVAL) {
      relay2Status = false;
      relay2Manual = false;
      setRelay(RELAY2, false);
      lastRelay2Off = millis();
      if (WiFi.status() == WL_CONNECTED) Blynk.virtualWrite(V2, 0);
      Serial.println("Relay 2 OFF (Auto 05:30)");
    }

    lastTimeCheck = millis();
  }
}
