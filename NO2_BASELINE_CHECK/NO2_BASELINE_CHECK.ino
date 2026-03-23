#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_ADS1115 ads;

const int   SAMPLES = 120;
const float ADS_LSB = 0.0078125 / 1000.0; // GAIN_SIXTEEN

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  ads.begin(0x48);
  ads.setGain(GAIN_SIXTEEN);

  Serial.println("========= NO2 BASELINE CALIBRATION =========");
  delay(3000);

  long rawSum = 0;
  float voltSum = 0;

  for (int i = 1; i <= SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(2); // A1
    float voltage = raw * ADS_LSB;

    rawSum += raw;
    voltSum += voltage;

    Serial.print("Sample ");
    Serial.print(i);
    Serial.print(" → ");
    Serial.print(voltage * 1000.0, 3);
    Serial.println(" mV");

    delay(1000);
  }

  Serial.println("----------------------------------");
  Serial.print("✅ NO2 BASELINE RAW AVG : ");
  Serial.println(rawSum / (float)SAMPLES, 2);

  Serial.print("✅ NO2 BASELINE V0      : ");
  Serial.print((voltSum / SAMPLES) * 1000.0, 3);
  Serial.println(" mV");

  Serial.println("📌 SAVE THIS VALUE");
}

void loop() {}
