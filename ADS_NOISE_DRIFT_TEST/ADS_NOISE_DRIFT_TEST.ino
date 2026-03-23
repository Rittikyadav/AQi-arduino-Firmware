#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_ADS1115 ads;
const float ADS_LSB = 0.125 / 1000.0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  ads.begin(0x48);
  ads.setGain(GAIN_ONE);

  Serial.println("=== ADS1115 STABILITY TEST ===");
}

void loop() {
  int16_t raw = ads.readADC_SingleEnded(0);
  float voltage = raw * ADS_LSB;

  Serial.print("ADC Raw: ");
  Serial.print(raw);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 4);
  Serial.println(" V");

  delay(1000);
}
