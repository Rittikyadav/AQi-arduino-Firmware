#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

#define TOTAL_SAMPLES 200

float sumNH3 = 0;
float sumNO2 = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21,22);

  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not detected!");
    while (1);
  }

  ads.setGain(GAIN_ONE);

  Serial.println("=== NH3 & NO2 BASELINE CALIBRATION ===");
  Serial.println("Keep sensor in CLEAN AIR");
}

void loop() {

  for(int i = 1; i <= TOTAL_SAMPLES; i++) {

    float vNH3 = ads.computeVolts(ads.readADC_SingleEnded(1));
    float vNO2 = ads.computeVolts(ads.readADC_SingleEnded(2));

    Serial.print("Sample ");
    Serial.print(i);

    Serial.print(" | NH3: ");
    Serial.print(vNH3,6);

    Serial.print(" V | NO2: ");
    Serial.print(vNO2,6);
    Serial.println(" V");

    sumNH3 += vNH3;
    sumNO2 += vNO2;

    delay(2000);  // 2 sec delay
  }

  float avgNH3 = sumNH3 / TOTAL_SAMPLES;
  float avgNO2 = sumNO2 / TOTAL_SAMPLES;

  Serial.println("\n===== FINAL BASELINE =====");

  Serial.print("NH3_BASELINE_V0 = ");
  Serial.println(avgNH3,6);

  Serial.print("NO2_BASELINE_V0 = ");
  Serial.println(avgNO2,6);

  Serial.println("\nCopy these values into main code");

  while(1);
}
