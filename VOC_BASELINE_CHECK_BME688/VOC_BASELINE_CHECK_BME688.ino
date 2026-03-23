#include <Wire.h>
#include <Adafruit_BME680.h>

// I2C Pins
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_BME680 bme;

// Calibration settings
#define VOC_SAMPLES 60
#define SAMPLE_DELAY_MS 2000

float vocSum = 0;
int sampleCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!bme.begin(0x76)) {
    Serial.println("❌ BME688 not detected");
    while (1);
  }

  // Recommended stable configuration
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  Serial.println("======================================");
  Serial.println(" BME688 VOC BASELINE CALIBRATION");
  Serial.println(" Keep sensor in CLEAN AIR");
  Serial.println("======================================");
}

void loop() {
  if (!bme.performReading()) return;

  float gasRes_kOhm = bme.gas_resistance / 1000.0;
  vocSum += gasRes_kOhm;
  sampleCount++;

  Serial.print("Sample ");
  Serial.print(sampleCount);
  Serial.print(" → Gas Resistance: ");
  Serial.print(gasRes_kOhm, 2);
  Serial.println(" kΩ");

  if (sampleCount >= VOC_SAMPLES) {
    float vocBaseline = vocSum / VOC_SAMPLES;

    Serial.println("--------------------------------------");
    Serial.print("✅ VOC BASELINE R0 (kΩ): ");
    Serial.println(vocBaseline, 2);
    Serial.println("📌 SAVE THIS VALUE");
    Serial.println("======================================");

    while (1); // stop
  }

  delay(SAMPLE_DELAY_MS);
}
