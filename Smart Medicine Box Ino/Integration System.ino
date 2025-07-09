#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// === WiFi Credentials ===
const char* ssid = "Pixel69";
const char* password = "qazmlp10";

// === NTP Configuration ===
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // WIB = UTC+7
const int daylightOffset_sec = 0;     // No DST in Indonesia
const long ntpSyncInterval = 24 * 3600 * 1000; // Sync NTP every 24 hours
unsigned long lastNTPSyncMillis = 0;

// === Firebase Config ===
#define API_KEY "AIzaSyD1vftAoTLafxLu2xZc66_DgwpGbNcBbZ0"
#define DATABASE_URL "https://esp32-ldr-3f867-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "masuk@gmail.com"
#define USER_PASSWORD "masukcuy"

// === Periodic Logging ===
const long logInterval = 30000; // Log time every 30 seconds
unsigned long lastLogMillis = 0;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

RTC_DS3231 rtc;

// === Shift Register untuk LED ===
const int dataPin = 13;
const int clockPin = 12;
const int latchPin = 14;
uint32_t ledStates = 0;

// === Multiplexer untuk LDR (74HC4067) ===
const int mux_s0_4067 = 15;
const int mux_s1_4067 = 2;
const int mux_s2_4067 = 4;
const int mux_s3_4067 = 5;
const int ldrInputPin_4067 = 34;

// === Multiplexer untuk LDR (74HC4051) ===
const int mux_s0_4051 = 25;
const int mux_s1_4051 = 26;
const int mux_s2_4051 = 27;
const int ldrInputPin_4051 = 35;

// === Buzzer ===
const int buzzerPin = 23;

// === LCD ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === Struktur Alarm ===
struct Alarm {
  int hour;
  int minute;
  int ledId;
  bool active;
  bool triggeredToday;
};

const int maxAlarms = 3;
Alarm alarms[maxAlarms];
int alarmCount = 0;

bool sudahDiambil = false;
bool buzzerOn = false;
unsigned long buzzerStart = 0;
unsigned long lastBuzz = 0;
int currentLED = -1;
bool alarmActive = false;

// Variable to track the last day an alarm reset occurred
int lastAlarmResetDay = -1;

// Variable to control how often Firebase is read
unsigned long lastFirebaseReadMillis = 0;
const long firebaseReadInterval = 30000; // Read Firebase every 30 seconds

const char* weekdayStr(int day) {
  switch (day) {
    case 0: return "Sunday";
    case 1: return "Monday";
    case 2: return "Tuesday";
    case 3: return "Wednesday";
    case 4: return "Thursday";
    case 5: return "Friday";
    case 6: return "Saturday";
    default: return "Unknown";
  }
}

void syncTimeFromNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot sync with NTP: WiFi not connected");
    return;
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time from NTP");
    return;
  }
  Serial.println("NTP time synchronized");
  // Update RTC with NTP time
  rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));
  lastNTPSyncMillis = millis();
}

void syncSystemTimeFromRTC() {
  DateTime now = rtc.now();
  struct tm timeinfo;
  timeinfo.tm_year = now.year() - 1900;
  timeinfo.tm_mon  = now.month() - 1;
  timeinfo.tm_mday = now.day();
  timeinfo.tm_hour = now.hour();
  timeinfo.tm_min  = now.minute();
  timeinfo.tm_sec  = now.second();
  timeinfo.tm_isdst = 0;
  time_t t = mktime(&timeinfo);
  struct timeval now_tv = { .tv_sec = t };
  settimeofday(&now_tv, NULL);
}

void logTime() {
  DateTime rtcTime = rtc.now();
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  Serial.printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                rtcTime.year(), rtcTime.month(), rtcTime.day(),
                rtcTime.hour(), rtcTime.minute(), rtcTime.second());
  Serial.printf("System Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void loadAlarmsFromFirebase() {
  bool currentTriggeredStates[maxAlarms];
  for (int i = 0; i < maxAlarms; i++) {
    currentTriggeredStates[i] = alarms[i].triggeredToday;
  }

  alarmCount = 0;
  for (int i = 0; i < maxAlarms; i++) {
    String path = "/alarms/" + String(i);
    if (Firebase.getJSON(fbdo, path)) {
      FirebaseJson &json = fbdo.jsonObject();
      FirebaseJsonData result;
      Alarm a;

      json.get(result, "hour"); a.hour = result.intValue;
      json.get(result, "minute"); a.minute = result.intValue;
      json.get(result, "ledId"); a.ledId = result.intValue;

      a.active = true;
      a.triggeredToday = currentTriggeredStates[i];
      alarms[i] = a;
      alarmCount++;
      Serial.printf("Alarm %d loaded: %02d:%02d LED %d (Triggered Today: %s)\n",
                    i, a.hour, a.minute, a.ledId, a.triggeredToday ? "True" : "False");
    } else {
      alarms[i].active = false;
      alarms[i].hour = 0;
      alarms[i].minute = 0;
      alarms[i].ledId = 0;
      alarms[i].triggeredToday = false;
      Serial.printf("Alarm %d not found or failed to load from Firebase\n", i);
    }
  }
}

void sendHistoryToFirebase(int ledId, const String& status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send to Firebase: WiFi not connected");
    return;
  }

  DateTime now = rtc.now();
  char path[100];
  snprintf(path, sizeof(path), "/history/%04d-%02d-%02d-%s_%d",
           now.year(), now.month(), now.day(), weekdayStr(now.dayOfTheWeek()), ledId);

  char timestamp[6];
  snprintf(timestamp, sizeof(timestamp), "%02d:%02d", now.hour(), now.minute());

  FirebaseJson json;
  json.set("ledId", ledId);
  json.set("status", status);
  json.set("timestamp", timestamp);

  int retryCount = 0;
  const int maxRetries = 3;
  while (retryCount < maxRetries) {
    if (Firebase.setJSON(fbdo, path, json)) {
      Serial.printf("History sent: LED %d, Status %s\n", ledId, status.c_str());
      return;
    }
    Serial.println("Failed to send history: " + fbdo.errorReason());
    retryCount++;
    delay(1000);
  }
  Serial.println("Failed to send history after " + String(maxRetries) + " attempts");
}

int readLDR(int channel) {
  if (channel < 16) {
    digitalWrite(mux_s0_4067, bitRead(channel, 0));
    digitalWrite(mux_s1_4067, bitRead(channel, 1));
    digitalWrite(mux_s2_4067, bitRead(channel, 2));
    digitalWrite(mux_s3_4067, bitRead(channel, 3));
    delay(5);
    return analogRead(ldrInputPin_4067);
  } else {
    int adjustedChannel = channel - 16;
    digitalWrite(mux_s0_4051, bitRead(adjustedChannel, 0));
    digitalWrite(mux_s1_4051, bitRead(adjustedChannel, 1));
    digitalWrite(mux_s2_4051, bitRead(adjustedChannel, 2));
    delay(5);
    return analogRead(ldrInputPin_4051);
  }
}

void setLED(int ledId, bool state) {
  if (ledId < 0 || ledId >= 21) return; // Prevent invalid LED IDs
  if (state)
    bitSet(ledStates, ledId);
  else
    bitClear(ledStates, ledId);

  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, (ledStates >> 16) & 0xFF);
  shiftOut(dataPin, clockPin, MSBFIRST, (ledStates >> 8) & 0xFF);
  shiftOut(dataPin, clockPin, MSBFIRST, ledStates & 0xFF);
  digitalWrite(latchPin, HIGH);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC initialization failed!");
    while (1); // Halt if RTC fails
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting to compile time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize alarms
  for (int i = 0; i < maxAlarms; i++) {
    alarms[i].triggeredToday = false;
    alarms[i].active = false;
  }

  // Initialize pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(mux_s0_4067, OUTPUT);
  pinMode(mux_s1_4067, OUTPUT);
  pinMode(mux_s2_4067, OUTPUT);
  pinMode(mux_s3_4067, OUTPUT);
  pinMode(ldrInputPin_4067, INPUT);
  pinMode(mux_s0_4051, OUTPUT);
  pinMode(mux_s1_4051, OUTPUT);
  pinMode(mux_s2_4051, OUTPUT);
  pinMode(ldrInputPin_4051, INPUT);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" Smart Pillbox ");
  lcd.setCursor(0, 1);
  lcd.print("   Initializing...");
  delay(2000);
  lcd.clear();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  // Sync time from NTP
  syncTimeFromNTP();
  syncSystemTimeFromRTC();

  // Initialize Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  // Initialize LEDs
  ledStates = 0;
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0x00);
  shiftOut(dataPin, clockPin, MSBFIRST, 0x00);
  shiftOut(dataPin, clockPin, MSBFIRST, 0x00);
  digitalWrite(latchPin, HIGH);

  // Initial load of alarms
  loadAlarmsFromFirebase();
  lastAlarmResetDay = rtc.now().day();
}

void loop() {
  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Reconnected");
      syncTimeFromNTP(); // Sync time after reconnecting
    } else {
      Serial.println("\nWiFi Reconnection Failed");
    }
  }

  // Sync NTP periodically
  if (millis() - lastNTPSyncMillis >= ntpSyncInterval) {
    syncTimeFromNTP();
    syncSystemTimeFromRTC();
  }

  // Log time periodically
  if (millis() - lastLogMillis >= logInterval) {
    logTime();
    lastLogMillis = millis();
  }

  DateTime now = rtc.now();

  // Update LCD with time and WiFi status
  lcd.setCursor(0, 0);
  lcd.printf("%02d/%02d/%04d %02d:%02d %c", now.day(), now.month(), now.year(),
             now.hour(), now.minute(), WiFi.status() == WL_CONNECTED ? 'W' : 'X');

  // Reset triggeredToday and update ledId on new day
  if (now.day() != lastAlarmResetDay) {
    Serial.println("New day detected, resetting triggeredToday and updating ledId.");
    for (int i = 0; i < maxAlarms; i++) {
      if (alarms[i].active) {
        alarms[i].triggeredToday = false;
        alarms[i].ledId = (alarms[i].ledId + 3) % 21; // Move to next LED (step by 3)
        FirebaseJson json;
        json.set("hour", alarms[i].hour);
        json.set("minute", alarms[i].minute);
        json.set("ledId", alarms[i].ledId);
        String path = "/alarms/" + String(i);
        if (Firebase.setJSON(fbdo, path, json)) {
          Serial.printf("Updated LedId for alarm %d to %d in Firebase\n", i, alarms[i].ledId);
        } else {
          Serial.println("Failed to update LedId in Firebase: " + fbdo.errorReason());
        }
      }
    }
    lastAlarmResetDay = now.day();
  }

  // Periodically load alarms from Firebase
  if (millis() - lastFirebaseReadMillis >= firebaseReadInterval) {
    loadAlarmsFromFirebase();
    lastFirebaseReadMillis = millis();
  }

  // Read LDR values
  int ldrValues[21];
  for (int i = 0; i < 21; i++) {
    ldrValues[i] = readLDR(i);
  }

  // Check alarms
  for (int i = 0; i < alarmCount; i++) {
    Alarm &a = alarms[i];
    if (!a.active) continue;

    if (now.hour() == a.hour && now.minute() == a.minute && !a.triggeredToday) {
      alarmActive = true;
      currentLED = a.ledId;
      setLED(currentLED, true);
      digitalWrite(buzzerPin, HIGH);
      delay(5000);
      digitalWrite(buzzerPin, LOW);
      lastBuzz = millis();
      buzzerOn = false;
      sudahDiambil = false;
      a.triggeredToday = true;
      Serial.printf("Alarm triggered for LED %d at %02d:%02d\n", a.ledId, a.hour, a.minute);
    }
  }

  // Handle active alarm
  if (alarmActive) {
    lcd.setCursor(0, 1);
    lcd.printf("Alarm: Loker %2d", currentLED);
    int ldrValue = ldrValues[currentLED];

    if (ldrValue < 2500 && !sudahDiambil) {
      sudahDiambil = true;
      setLED(currentLED, false);
      sendHistoryToFirebase(currentLED, "diambil");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Obat Diambil");
      lcd.setCursor(0, 1);
      lcd.printf("Loker: %2d", currentLED);
      alarmActive = false;
      buzzerOn = false;
      digitalWrite(buzzerPin, LOW);
      currentLED = -1;
      delay(2000); // Show message for 2 seconds
      lcd.clear();
    } else if (sudahDiambil && ldrValue > 2500) {
      sendHistoryToFirebase(currentLED, "dikembalikan");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Obat Kembali");
      lcd.setCursor(0, 1);
      lcd.printf("Loker: %2d", currentLED);
      alarmActive = false;
      sudahDiambil = false;
      currentLED = -1;
      delay(2000);
      lcd.clear();
    } else if (!buzzerOn && millis() - lastBuzz >= 15000) {
      digitalWrite(buzzerPin, HIGH);
      buzzerOn = true;
      buzzerStart = millis();
    }

    if (buzzerOn && millis() - buzzerStart >= 5000) {
      digitalWrite(buzzerPin, LOW);
      buzzerOn = false;
      lastBuzz = millis();
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Status: Standby ");
  }

  delay(100); // Reduce delay for better responsiveness
}