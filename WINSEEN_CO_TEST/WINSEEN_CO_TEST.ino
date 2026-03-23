#define RXD2 16
#define TXD2 17

HardwareSerial coSerial(2);

byte buffer[9];

void setup() {
  Serial.begin(115200);
  coSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  // Sync to start byte
  if (coSerial.available()) {
    if (coSerial.read() == 0xFF) {

      buffer[0] = 0xFF;

      // wait for remaining 8 bytes
      while (coSerial.available() < 8);

      coSerial.readBytes(&buffer[1], 8);

      // Print raw
      Serial.print("RAW: ");
      for (int i = 0; i < 9; i++) {
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Validate
      if (buffer[1] == 0x04 && buffer[2] == 0x03) {

        int co_ppm = buffer[4] * 256 + buffer[5];

        Serial.print("CO: ");
        Serial.print(co_ppm);
        Serial.println(" ppm");

      } else {
        Serial.println("Frame error");
      }
    }
  }
}
