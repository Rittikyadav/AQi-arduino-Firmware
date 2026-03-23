#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_ADS1115 ads;

const int   SAMPLES = 120;           // 2 minutes (1 sample/sec)
const float ADS_LSB = 0.125 / 1000.0; // GAIN_ONE → V/LSB

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 not detected");
    while (1);
  }

  ads.setGain(GAIN_ONE);

  Serial.println("======================================");
  Serial.println(" CO SENSOR BASELINE CALIBRATION ");
  Serial.println(" Keep sensor in CLEAN AIR ");
  Serial.println(" Do NOT breathe near sensor ");
  Serial.println("======================================");
  delay(3000);

  long rawSum = 0;
  float voltSum = 0;

  for (int i = 1; i <= SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(0);
    float voltage = raw * ADS_LSB;

    rawSum  += raw;
    voltSum += voltage;

    Serial.print("Sample ");
    Serial.print(i);
    Serial.print(" → RAW: ");
    Serial.print(raw);
    Serial.print(" | V: ");
    Serial.print(voltage, 4);
    Serial.println(" V");

    delay(1000);
  }

  float rawAvg  = rawSum / (float)SAMPLES;
  float voltAvg = voltSum / SAMPLES;

  Serial.println("--------------------------------------");
  Serial.print("✅ CO BASELINE RAW AVG : ");
  Serial.println(rawAvg, 2);
  Serial.print("✅ CO BASELINE V0      : ");
  Serial.print(voltAvg, 4);
  Serial.println(" V");
  Serial.println("📌 SAVE THIS V0 VALUE");
  Serial.println("======================================");
}

void loop() {}
