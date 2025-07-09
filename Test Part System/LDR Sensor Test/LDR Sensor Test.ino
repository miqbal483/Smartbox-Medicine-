// Definisi pin untuk multiplexer 74HC4067
const int s0_4067 = 15; // Pin kontrol S0 untuk 74HC4067
const int s1_4067 = 2;  // Pin kontrol S1 untuk 74HC4067
const int s2_4067 = 4;  // Pin kontrol S2 untuk 74HC4067
const int s3_4067 = 5;  // Pin kontrol S3 untuk 74HC4067
const int sigPin_4067 = 34; // Pin analog untuk membaca output SIG 74HC4067

// Definisi pin untuk multiplexer 74HC4051
const int s0_4051 = 25; // Pin kontrol S0 untuk 74HC4051
const int s1_4051 = 26; // Pin kontrol S1 untuk 74HC4051
const int s2_4051 = 27; // Pin kontrol S2 untuk 74HC4051
const int sigPin_4051 = 35; // Pin analog untuk membaca output SIG 74HC4051

// Array untuk menyimpan nilai ADC dari 21 saluran
int ldrValues[21];

void setup() {
  Serial.begin(115200); // Inisialisasi Serial Monitor

  // Set pin kontrol 74HC4067 sebagai output
  pinMode(s0_4067, OUTPUT);
  pinMode(s1_4067, OUTPUT);
  pinMode(s2_4067, OUTPUT);
  pinMode(s3_4067, OUTPUT);

  // Set pin kontrol 74HC4051 sebagai output
  pinMode(s0_4051, OUTPUT);
  pinMode(s1_4051, OUTPUT);
  pinMode(s2_4051, OUTPUT);

  // Inisialisasi pin SIG sebagai input
  pinMode(sigPin_4067, INPUT);
  pinMode(sigPin_4051, INPUT);

  Serial.println("Mulai pembacaan 21 LDR dengan 74HC4067 dan 74HC4051...");
  delay(1000);
}

void loop() {
  // Baca 16 saluran dari 74HC4067 (indeks 0-15)
  for (int i = 0; i < 16; i++) {
    // Pilih saluran dengan kombinasi bit
    digitalWrite(s0_4067, (i & 0x01) ? HIGH : LOW); // Bit 0
    digitalWrite(s1_4067, (i & 0x02) ? HIGH : LOW); // Bit 1
    digitalWrite(s2_4067, (i & 0x04) ? HIGH : LOW); // Bit 2
    digitalWrite(s3_4067, (i & 0x08) ? HIGH : LOW); // Bit 3

    // Baca nilai analog dari pin SIG 74HC4067
    ldrValues[i] = analogRead(sigPin_4067);

    // Tampilkan hasil di Serial Monitor
    Serial.print("LDR ");
    Serial.print(i + 1); // Mulai dari LDR 1 hingga LDR 16
    Serial.print(": ");
    Serial.println(ldrValues[i]);

    delay(2000); // Penundaan kecil untuk stabilitas
  }

  // Baca 5 saluran dari 74HC4051 (indeks 16-20)
  for (int i = 0; i < 5; i++) {
    int channel = i; // Gunakan indeks 0-4 untuk 74HC4051 (hanya 5 saluran dari 8)
    // Pilih saluran dengan kombinasi bit
    digitalWrite(s0_4051, (channel & 0x01) ? HIGH : LOW); // Bit 0
    digitalWrite(s1_4051, (channel & 0x02) ? HIGH : LOW); // Bit 1
    digitalWrite(s2_4051, (channel & 0x04) ? HIGH : LOW); // Bit 2

    // Baca nilai analog dari pin SIG 74HC4051
    ldrValues[i + 16] = analogRead(sigPin_4051);

    // Tampilkan hasil di Serial Monitor
    Serial.print("LDR ");
    Serial.print(i + 17); // Mulai dari LDR 17 hingga LDR 21
    Serial.print(": ");
    Serial.println(ldrValues[i + 16]);

    delay(2000); // Penundaan kecil untuk stabilitas
  }

  Serial.println("------------------------");
  delay(1500); // Tunggu 1 detik sebelum pembacaan berikutnya
}

// Fungsi untuk mengatur saluran multiplexer (opsional, sudah digantikan oleh logika di loop)
void setChannel4067(int channel) {
  digitalWrite(s0_4067, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(s1_4067, (channel & 0x02) ? HIGH : LOW);
  
  digitalWrite(s2_4067, (channel & 0x04) ? HIGH : LOW);
  digitalWrite(s3_4067, (channel & 0x08) ? HIGH : LOW);
}

void setChannel4051(int channel) {
  digitalWrite(s0_4051, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(s1_4051, (channel & 0x02) ? HIGH : LOW);
  digitalWrite(s2_4051, (channel & 0x04) ? HIGH : LOW);
}