/***************************************************
 * OXYGEN SENSOR STANDALONE TEST (UART)
 * Resolution : 0.1 %
 * Output     : Oxygen % + Status
 * Board      : ESP32
 ***************************************************/

#define RXD2 16
#define TXD2 17

uint8_t o2Buffer[12];
float oxygenPercent = -1;

/* -------------------------------
   Oxygen status classification
-------------------------------- */
String oxygenStatus(float o2) {
  if (o2 < 19.5) return "LOW (UNSAFE)";
  if (o2 < 20.5) return "BELOW NORMAL";
  if (o2 <= 21.5) return "NORMAL";
  return "ABOVE NORMAL";
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("=================================");
  Serial.println(" OXYGEN SENSOR (O2) CHECK MODE ");
  Serial.println("=================================");
  Serial.println("Waiting for valid O2 data...\n");
}

void loop() {

  while (Serial2.available()) {
    // Shift buffer
    for (int i = 0; i < 11; i++) {
      o2Buffer[i] = o2Buffer[i + 1];
    }
    o2Buffer[11] = Serial2.read();

    // Frame validation
    if (o2Buffer[0] == 0x16 &&
        o2Buffer[1] == 0x09 &&
        o2Buffer[2] == 0x01) {

      uint16_t raw = o2Buffer[4] | (o2Buffer[5] << 8);
      float o2 = raw / 10.0;

      // Sanity check
      if (o2 >= 18.0 && o2 <= 25.0) {
        oxygenPercent = o2;
      }
    }
  }

  // Display result
  if (oxygenPercent > 0) {
    Serial.println("---------------------------------");
    Serial.print("🫁 Oxygen Concentration : ");
    Serial.print(oxygenPercent, 1);
    Serial.println(" %");

    Serial.print("📊 Status               : ");
    Serial.println(oxygenStatus(oxygenPercent));
    Serial.println("---------------------------------\n");
  }

  delay(2000);
}
