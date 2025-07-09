#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C 0x27, 16 kolom, 2 baris

const int buzzerPin = 23;
const int ledPin = 2;

// Ganti dengan SSID dan password Wi-Fi Anda
const char* ssid = "Pixel69";
const char* password = "qazmlp10";

const int maxAlarms = 3;
const unsigned long alarmDuration = 5000UL; // Durasi buzzer dan LED menyala 5 detik

struct Alarm {
  int hour;
  int minute;10
  bool active;
  bool triggeredToday;
};

Alarm alarms[maxAlarms];
int alarmCount = 0;

bool alarmActive = false;
unsigned long alarmStart = 0;
bool isInitialized = false; // Flag untuk inisialisasi sekali

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, LOW);

  lcd.begin(16, 2); // Inisialisasi LCD dengan 16 kolom dan 2 baris
  lcd.backlight(); // Nyalakan backlight
  lcd.setCursor(0, 0);
  lcd.print("Sistem Mulai...");

  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi!");
    lcd.setCursor(0, 1);
    lcd.print("RTC Error!");
    while (1);
  }

  if (!isInitialized) {
    lcd.setCursor(0, 1);
    lcd.print("Connecting WiFi...");
    WiFi.begin(ssid, password);
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nTerhubung ke Wi-Fi!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Connected");

      // Sinkronisasi waktu dengan NTP
      configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com"); // WIB UTC+7
      delay(2000); // Tunggu sinkronisasi NTP
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
        Serial.println("Waktu RTC disinkronkan dengan NTP!");
        lcd.setCursor(0, 1);
        lcd.print("NTP Sinkron!");
      } else {
        Serial.println("Gagal sinkronisasi NTP, menggunakan waktu kompilasi...");
        lcd.setCursor(0, 1);
        lcd.print("NTP Gagal!");
        if (rtc.lostPower()) {
          rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
      }
    } else {
      Serial.println("\nGagal terhubung ke Wi-Fi, lanjut offline...");
      lcd.setCursor(0, 1);
      lcd.print("WiFi Gagal!");
      if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
    }

    delay(1000); // Kurangi penundaan
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time: --:--:--");
    isInitialized = true; // Tandai inisialisasi selesai
  }

  Serial.println("\n=== Multi-Alarm + LCD ===");
  Serial.println("Masukkan alarm (format HH:MM), maksimal 3 alarm.");
}

void loop() {
  DateTime now = rtc.now();
  static int lastSecond = -1;

  if (now.second() != lastSecond) {
    lastSecond = now.second();
    // Tampilan konsisten untuk LCD dan Serial
    lcd.setCursor(0, 0);
    lcd.printf("Time:%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    Serial.printf("Time:%02d:%02d:%02d", now.hour(), now.minute(), now.second());

    // Tampilkan status koneksi
    if (WiFi.status() == WL_CONNECTED) {
      lcd.setCursor(11, 0);
      lcd.print("1");
      Serial.print(" [1]");
    } else {
      lcd.setCursor(11, 0);
      lcd.print("2");
      Serial.print(" [2]");
    }
    Serial.println();
  }

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (alarmCount < maxAlarms && parseAlarm(line, alarms[alarmCount])) {
      alarms[alarmCount].active = true;
      alarms[alarmCount].triggeredToday = false;
      Serial.printf("Alarm %d ditambahkan: %02d:%02d\n",
                    alarmCount + 1, alarms[alarmCount].hour, alarms[alarmCount].minute);
      lcd.setCursor(0, 1);
      lcd.printf("Alarm %d: %02d:%02d", alarmCount + 1, alarms[alarmCount].hour, alarms[alarmCount].minute);
      delay(1000); // Kurangi penundaan
      lcd.setCursor(0, 1);
      lcd.print("                "); // Bersihkan baris kedua
      alarmCount++;
    } else {
      Serial.println("Format salah atau jumlah alarm maksimum tercapai.");
      lcd.setCursor(0, 1);
      lcd.print("Format Salah!");
      delay(1000); // Kurangi penundaan
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }

  // Periksa alarm
  for (int i = 0; i < alarmCount; i++) {
    Alarm &a = alarms[i];
    if (!a.active) continue;

    if (now.hour() == a.hour && now.minute() == a.minute && !a.triggeredToday) {
      Serial.printf(">>Alarm %02d:%02d:%02d AKTIF\n", now.hour(), now.minute(), now.second());
      lcd.setCursor(0, 1);
      lcd.print("Alarm Aktif!");

      alarmActive = true;
      alarmStart = millis();
      digitalWrite(buzzerPin, HIGH);
      digitalWrite(ledPin, HIGH);

      a.triggeredToday = true;
    }

    if (now.hour() != a.hour || now.minute() != a.minute) {
      a.triggeredToday = false;
    }
  }

  // Matikan alarm setelah 5 detik
  if (alarmActive && (millis() - alarmStart >= alarmDuration)) {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
    alarmActive = false;
    lcd.setCursor(0, 1);
    lcd.print("Alarm Selesai");
    delay(1000); // Kurangi penundaan
    lcd.setCursor(0, 1);
    lcd.print("");
    Serial.println("Alarm Selesai");
  }

  delay(100); // Kurangi penundaan utama untuk pembaruan lebih cepat
}

bool parseAlarm(const String &s, Alarm &a) {
  if (s.length() != 5 || s.charAt(2) != ':') return false;
  int h = s.substring(0, 2).toInt();
  int m = s.substring(3, 5).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  a.hour = h;
  a.minute = m;
  return true;
}