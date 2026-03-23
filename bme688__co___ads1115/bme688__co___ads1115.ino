#include <Wire.h>
#include "bsec.h"
#include <Adafruit_ADS1X15.h>

// =====================
// I2C CONFIG
// =====================
#define SDA_PIN 21
#define SCL_PIN 22

// =====================
// OBJECTS
// =====================
Bsec iaqSensor;
Adafruit_ADS1115 ads;

// =====================
// CPCB IAQ CATEGORY
// =====================
String cpcbCategory(float iaq) {
  if (iaq <= 50) return "GOOD (≤300 µg/m³)";
  else if (iaq <= 100) return "ACCEPTABLE (300–1000 µg/m³)";
  else if (iaq <= 150) return "POOR (1000–3000 µg/m³)";
  else return "SEVERE (>3000 µg/m³)";
}

// =====================
// CO SENSOR CONFIG
// =====================
const float CO_MAX = 5000.0;     // ppm
const float V_CO_MAX = 2.2;      // max voltage AFTER divider

float coBaseline = 0.55;         // initial guess (after divider)
const float CO_BASELINE_ALPHA = 0.003;

// =====================
// READ CO VOLTAGE (ADS1115)
// =====================
float readCOVoltage() {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += ads.readADC_SingleEnded(0);
    delay(5);
  }
  float raw = sum / 10.0;

  // GAIN_TWOTHIRDS → 0.1875 mV per bit
  return raw * 0.1875 / 1000.0;
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Wire.begin(SDA_PIN, SCL_PIN);

  // ---------- BME688 (BSEC v1) ----------
  iaqSensor.begin(BME68X_I2C_ADDR_LOW, Wire);

  bsec_virtual_sensor_t sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY
  };

  iaqSensor.updateSubscription(
    sensorList,
    sizeof(sensorList) / sizeof(sensorList[0]),
    BSEC_SAMPLE_RATE_LP
  );

  // ---------- ADS1115 ----------
  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 not detected");
    while (1);
  }
  ads.setGain(GAIN_TWOTHIRDS);   // ±6.144V

  Serial.println("✅ System started:");
  Serial.println("   • BME688 + BSEC v1");
  Serial.println("   • ADS1115 + CO sensor");
  Serial.println("⏳ Keep sensors in clean air for 5–10 minutes\n");
}

// =====================
// LOOP
// =====================
void loop() {

  // ---------- BME688 ----------
  if (iaqSensor.run()) {

    float iaq  = iaqSensor.iaq;
    float temp = iaqSensor.temperature;
    float hum  = iaqSensor.humidity;

    // ---------- CO SENSOR ----------
    float coVoltage = readCOVoltage();

    bool coSaturated = (coVoltage > 2.0);

    // Auto-baseline update (only in clean region)
    if (!coSaturated && coVoltage < (coBaseline + 0.02)) {
      coBaseline = (coBaseline * (1.0 - CO_BASELINE_ALPHA)) +
                   (coVoltage * CO_BASELINE_ALPHA);
    }

    float co_ppm = 0;
    if (!coSaturated) {
      co_ppm = (coVoltage - coBaseline) *
               (CO_MAX / (V_CO_MAX - coBaseline));

      if (co_ppm < 0) co_ppm = 0;
      if (co_ppm > CO_MAX) co_ppm = CO_MAX;
    }

    // ---------- PRINT ----------
    Serial.println("================================");

    Serial.print("🌡 Temperature : ");
    Serial.print(temp);
    Serial.println(" °C");

    Serial.print("💧 Humidity    : ");
    Serial.print(hum);
    Serial.println(" %");

    Serial.print("📊 IAQ Index   : ");
    Serial.println(iaq);

    Serial.print("🏛 IAQ Status  : ");
    Serial.println(cpcbCategory(iaq));

    Serial.println("--------------------------------");

    Serial.print("🧪 CO Voltage  : ");
    Serial.print(coVoltage, 3);
    Serial.println(" V");

    if (coSaturated) {
      Serial.println("⚠️ CO SENSOR SATURATED – RECOVERING");
    } else {
      Serial.print("🧪 CO Baseline : ");
      Serial.print(coBaseline, 3);
      Serial.println(" V");

      Serial.print("🧪 CO (ppm)    : ");
      Serial.print(co_ppm, 1);
      Serial.println(" ppm");
    }

    Serial.println("================================\n");

  } else {
    Serial.println("⏳ BSEC stabilizing...");
  }

  delay(3000);
}
