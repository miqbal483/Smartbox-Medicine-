// Definisikan pin untuk shift register
const int dataPin = 13;   // DS (Data) pin
const int clockPin = 12;  // SHCP (Clock) pin
const int latchPin = 14;  // STCP (Latch) pin

// Array untuk menyimpan status LED (0 = mati, 1 = nyala)
byte ledStates[21] = {0}; // 21 LED, diinisialisasi mati

void setup() {
  // Inisialisasi pin sebagai output
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);

  // Matikan semua LED awalnya
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0); // Shift register 3
  shiftOut(dataPin, clockPin, MSBFIRST, 0); // Shift register 2
  shiftOut(dataPin, clockPin, MSBFIRST, 0); // Shift register 1
  digitalWrite(latchPin, HIGH);

  Serial.begin(115200);
  Serial.println("Sistem LED Kotak Obat siap. Ketik 0-20 untuk nyalakan LED kotak, 21 untuk nyalakan semua, 22 untuk matikan semua.");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    int boxNumber = input.toInt();

    if (boxNumber >= 0 && boxNumber <= 22) {
      // Matikan semua LED terlebih dahulu
      for (int i = 0; i < 21; i++) ledStates[i] = 0;

      if (boxNumber >= 0 && boxNumber <= 20) {
        // Nyalakan LED untuk kotak yang dipilih
        ledStates[boxNumber] = 1;
        Serial.print("LED Kotak ");
        Serial.print(boxNumber);
        Serial.println(" menyala.");
      } else if (boxNumber == 21) {
        // Nyalakan semua LED
        for (int i = 0; i < 21; i++) ledStates[i] = 1;
        Serial.println("Semua LED menyala.");
      } else if (boxNumber == 22) {
        // Matikan semua LED (sudah dilakukan di awal)
        Serial.println("Semua LED dimatikan.");
      }
      updateLEDs();
    } else {
      Serial.println("Nomor kotak tidak valid. Gunakan 0-22.");
    }
    delay(100); // Penundaan kecil untuk stabilitas
  }
}

// Fungsi untuk memperbarui status LED ke shift register
void updateLEDs() {
  digitalWrite(latchPin, LOW);

  // Kirim data untuk shift register 3 (LED 16-20)
  byte thirdByte = 0;
  for (int i = 16; i < 21; i++) {
    if (ledStates[i]) bitSet(thirdByte, i - 16);
  }
  shiftOut(dataPin, clockPin, MSBFIRST, thirdByte);

  // Kirim data untuk shift register 2 (LED 8-15)
  byte secondByte = 0;
  for (int i = 8; i < 16; i++) {
    if (ledStates[i]) bitSet(secondByte, i - 8);
  }
  shiftOut(dataPin, clockPin, MSBFIRST, secondByte);

  // Kirim data untuk shift register 1 (LED 0-7)
  byte firstByte = 0;
  for (int i = 0; i < 8; i++) {
    if (ledStates[i]) bitSet(firstByte, i);
  }
  shiftOut(dataPin, clockPin, MSBFIRST, firstByte);

  digitalWrite(latchPin, HIGH);
}