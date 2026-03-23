#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_ADS1115 ads;

// ===== CONFIG =====
const float ADS_LSB = 0.125 / 1000.0;   // V per LSB
const float CO_BASELINE_V0 = 0.1076;    // your saved baseline

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 NOT FOUND");
    while (1);
  }

  ads.setGain(GAIN_ONE);

  Serial.println("====================================");
  Serial.println(" CO SENSOR + ADS1115 DIAGNOSTIC MODE ");
  Serial.println("====================================");
  Serial.println("ACTION: breathe near sensor OR use lighter (NO FLAME)");
  Serial.println("------------------------------------");
}

void loop() {
  int16_t raw = ads.readADC_SingleEnded(0);
  float voltage = raw * ADS_LSB;
  float deltaV = voltage - CO_BASELINE_V0;

  Serial.print("RAW: ");
  Serial.print(raw);

  Serial.print(" | Voltage: ");
  Serial.print(voltage, 4);
  Serial.print(" V");

  Serial.print(" | ΔV: ");
  Serial.print(deltaV * 1000.0, 2);
  Serial.println(" mV");

  delay(1000);
}
