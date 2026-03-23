#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

// =====================
// PIN CONFIG
// =====================
#define SDA_PIN 21
#define SCL_PIN 22

// =====================
// OBJECT
// =====================
Adafruit_BME680 bme;

// =====================
// GAS BASELINE
// =====================
#define GAS_SAMPLES 120   // ~4 minutes (120 × 2s)

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!bme.begin(0x76)) {
    Serial.println("❌ BME688 NOT DETECTED");
    while (1);
  }

  // Oversampling
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  // Gas heater
  bme.setGasHeater(320, 150);

  Serial.println("\n======================================");
  Serial.println(" BME688 STABILITY & BASELINE TEST");
  Serial.println("======================================");
  Serial.println("⏳ Allow sensor to stabilize...");
}

// =====================
// LOOP
// =====================
void loop() {

  static int sampleCount = 0;
  static float gasSum = 0;

  if (!bme.performReading()) {
    Serial.println("❌ BME688 read failed");
    delay(2000);
    return;
  }

  float temp = bme.temperature;
  float hum  = bme.humidity;
  float pres = bme.pressure / 100.0;
  float gasK = bme.gas_resistance / 1000.0;

  Serial.println("--------------------------------");
  Serial.print("🌡 Temperature (°C)  : "); Serial.println(temp, 2);
  Serial.print("💧 Humidity (%)      : "); Serial.println(hum, 2);
  Serial.print("⛰ Pressure (hPa)    : "); Serial.println(pres, 2);
  Serial.print("🧪 Gas Resistance   : "); Serial.print(gasK, 2); Serial.println(" kΩ");

  // Gas baseline accumulation
  if (sampleCount < GAS_SAMPLES) {
    gasSum += gasK;
    sampleCount++;

    Serial.print("📊 Baseline sample ");
    Serial.print(sampleCount);
    Serial.print(" / ");
    Serial.println(GAS_SAMPLES);
  }
  else if (sampleCount == GAS_SAMPLES) {
    float gasBaseline = gasSum / GAS_SAMPLES;

    Serial.println("======================================");
    Serial.print("✅ GAS BASELINE (kΩ): ");
    Serial.println(gasBaseline, 2);
    Serial.println("📌 SAVE THIS VALUE");
    Serial.println("======================================");

    sampleCount++; // lock
  }

  delay(2000);
}
