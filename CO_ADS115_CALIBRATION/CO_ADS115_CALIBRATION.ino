#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_ADS1115 ads;

// ADS1115 settings
const float ADS_LSB = 0.125 / 1000.0;   // GAIN_ONE → 0.125 mV per bit
const int   SAMPLES = 100;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ads.begin(0x48)) {
    Serial.println("ERROR: ADS1115 not detected");
    while (1);
  }

  ads.setGain(GAIN_ONE);

  Serial.println("--------------------------------------------");
  Serial.println("CO SENSOR BASELINE CALIBRATION (ADS1115)");
  Serial.println("Condition: Clean air, stable environment");
  Serial.println("--------------------------------------------");
  delay(2000);

  long rawSum = 0;
  int16_t rawMin = 32767;
  int16_t rawMax = -32768;

  for (int i = 1; i <= SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(0);

    rawSum += raw;
    if (raw < rawMin) rawMin = raw;
    if (raw > rawMax) rawMax = raw;

    float voltage = raw * ADS_LSB;

    Serial.print("Sample ");
    Serial.print(i);
    Serial.print(" -> RAW: ");
    Serial.print(raw);
    Serial.print(" | Voltage: ");
    Serial.print(voltage, 6);
    Serial.println(" V");

    delay(100);
  }

  float rawAvg = rawSum / (float)SAMPLES;
  float v0 = rawAvg * ADS_LSB;

  Serial.println("--------------------------------------------");
  Serial.println("CALIBRATION RESULT");
  Serial.println("--------------------------------------------");

  Serial.print("RAW Minimum      : ");
  Serial.println(rawMin);

  Serial.print("RAW Maximum      : ");
  Serial.println(rawMax);

  Serial.print("RAW Average      : ");
  Serial.println(rawAvg, 2);

  Serial.print("Baseline Voltage : ");
  Serial.print(v0, 6);
  Serial.println(" V");

  Serial.println("--------------------------------------------");
  Serial.println("SAVE THIS BASELINE:");
  Serial.println("• RAW Average → offset reference");
  Serial.println("• Voltage     → CO baseline (V0)");
  Serial.println("--------------------------------------------");
}

void loop() {
  // No loop needed for baseline calibration
}
