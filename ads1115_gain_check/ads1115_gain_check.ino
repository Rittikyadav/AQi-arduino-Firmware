#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// =====================
// I2C PINS (ESP32)
// =====================
#define SDA_PIN 21
#define SCL_PIN 22

// =====================
// OBJECT
// =====================
Adafruit_ADS1115 ads;

// =====================
// CONFIG
// =====================
#define NUM_SAMPLES 300

// =====================
// GAIN TABLE (for reference)
// GAIN_TWOTHIRDS : ±6.144V  (0.1875 mV/LSB)
// GAIN_ONE       : ±4.096V  (0.1250 mV/LSB)  <-- BEST FOR YOU
// GAIN_TWO       : ±2.048V  (0.0625 mV/LSB)
// =====================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 not detected");
    while (1);
  }

  // 🔧 SELECT GAIN
  ads.setGain(GAIN_ONE);  // ±4.096V (recommended)

  Serial.println("\n=======================================");
  Serial.println(" ADS1115 GAIN & BASELINE CALIBRATION");
  Serial.println("=======================================");
  Serial.println("Gain      : GAIN_ONE (±4.096V)");
  Serial.println("LSB       : 0.125 mV");
  Serial.println("Samples   : 300");
  Serial.println("---------------------------------------");
  Serial.println("⏳ Stabilizing... please wait 10 seconds");
  delay(10000);

  long rawSum = 0;
  float minRaw =  99999;
  float maxRaw = -99999;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(0);
    rawSum += raw;

    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;

    delay(10);
  }

  float rawAvg = rawSum / (float)NUM_SAMPLES;

  // Convert to voltage
  float lsb = 0.125 / 1000.0;   // volts
  float voltageAvg = rawAvg * lsb;

  Serial.println("\n========== CALIBRATION RESULT ==========");
  Serial.print("RAW Min      : "); Serial.println(minRaw);
  Serial.print("RAW Max      : "); Serial.println(maxRaw);
  Serial.print("RAW Average  : "); Serial.println(rawAvg, 2);

  Serial.print("Voltage Avg  : ");
  Serial.print(voltageAvg, 6);
  Serial.println(" V");

  Serial.println("---------------------------------------");
  Serial.println("📌 SAVE THIS RAW AVERAGE AS OFFSET");
  Serial.println("📌 SAVE THIS VOLTAGE AS BASELINE");
  Serial.println("=======================================");
}

void loop() {
  // Nothing here
}
