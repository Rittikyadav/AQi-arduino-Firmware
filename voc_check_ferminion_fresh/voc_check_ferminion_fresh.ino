#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

float VCC = 5.0;
float R0 = 7.5;  // calibrated

void setup() {
  Serial.begin(115200);

  if (!ads.begin()) {
    Serial.println("ADS1115 not found!");
    while (1);
  }

  ads.setGain(GAIN_ONE);
}

void loop() {
  int16_t adc = ads.readADC_SingleEnded(3);

  float voltage = adc * 0.125 / 1000.0;

  if (voltage < 0.05) return; // safety

  float Rs = (VCC - voltage) / voltage;
  float ratio = Rs / R0;

  float ppm = pow((R0 / Rs), 1.5) * 50;

  Serial.print("V: ");
  Serial.print(voltage, 4);

  Serial.print(" | Rs: ");
  Serial.print(Rs, 3);

  Serial.print(" | Ratio: ");
  Serial.print(ratio, 3);

  Serial.print(" | VOC: ");
  Serial.print(ppm);
  Serial.println(" ppm");

  delay(1000);
}
