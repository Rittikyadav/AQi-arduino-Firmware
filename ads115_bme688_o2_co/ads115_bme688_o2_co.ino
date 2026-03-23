#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_ADS1X15.h>

// ==================================================
// PIN CONFIG
// ==================================================
#define SDA_PIN 21
#define SCL_PIN 22
#define RXD2    16
#define TXD2    17

// ==================================================
// OBJECTS
// ==================================================
Adafruit_BME680 bme;
Adafruit_ADS1115 ads;

// ==================================================
// CO SENSOR (SEN0564) CONFIG
// ==================================================
const float CO_BASELINE_V0 = 0.1729;     // Clean-air baseline (calibrated)
const float ADS_LSB        = 0.125 / 1000.0; // ADS1115 GAIN_ONE (V/LSB)

// ==================================================
// FILTERING PARAMETERS
// ==================================================
#define VOC_WINDOW 5
#define CO_ALPHA   0.2     // IIR smoothing

float vocHistory[VOC_WINDOW] = {0};
int   vocIndex = 0;
float lastVocAvg = 0;

float coFiltered = 0;

// ==================================================
// OXYGEN SENSOR
// ==================================================
uint8_t o2Buf[12];
float oxygenValue = -1;

// ==================================================
// CO QUALITY CLASSIFICATION
// ==================================================
String coQuality(float ppm) {
  if (ppm <= 9)   return "EXCELLENT";
  if (ppm <= 35)  return "ACCEPTABLE";
  if (ppm <= 100) return "WARNING";
  if (ppm <= 300) return "DANGEROUS";
  return "CRITICAL";
}

String coRiskNote(String level) {
  if (level == "EXCELLENT")  return "Fresh air";
  if (level == "ACCEPTABLE") return "Normal indoor";
  if (level == "WARNING")    return "Ventilation advised";
  if (level == "DANGEROUS")  return "Health risk";
  return "IMMEDIATE ACTION";
}

// ==================================================
// VOC INTERPRETATION (BME688)
// ==================================================
String vocLevel(float r) {
  if (r > 50) return "CLEAN";
  if (r > 30) return "GOOD";
  if (r > 15) return "MODERATE";
  return "POOR";
}

String vocTrend(float current, float previous) {
  if (current > previous + 1.0) return "IMPROVING";
  if (current < previous - 1.0) return "DEGRADING";
  return "STABLE";
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // BME688
  if (!bme.begin(0x76)) {
    Serial.println("❌ BME688 not detected");
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  // ADS1115
  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 not detected");
    while (1);
  }
  ads.setGain(GAIN_ONE);

  // Oxygen sensor UART
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\n==============================================");
  Serial.println(" AIR QUALITY MONITOR SYSTEM (DEMO BUILD)");
  Serial.println("==============================================");
}

// ==================================================
// LOOP
// ==================================================
void loop() {

  // ------------------------------------------------
  // OXYGEN SENSOR
  // ------------------------------------------------
  while (Serial2.available()) {
    for (int i = 0; i < 11; i++) o2Buf[i] = o2Buf[i + 1];
    o2Buf[11] = Serial2.read();

    if (o2Buf[0] == 0x16 && o2Buf[1] == 0x09 && o2Buf[2] == 0x01) {
      uint16_t raw = o2Buf[4] | (o2Buf[5] << 8);
      float o2 = raw / 10.0;
      if (o2 >= 18.0 && o2 <= 25.0) oxygenValue = o2;
    }
  }

  // ------------------------------------------------
  // BME688 READ
  // ------------------------------------------------
  if (!bme.performReading()) return;

  float vocRes = bme.gas_resistance / 1000.0;

  vocHistory[vocIndex++] = vocRes;
  if (vocIndex >= VOC_WINDOW) vocIndex = 0;

  float vocAvg = 0;
  for (int i = 0; i < VOC_WINDOW; i++) vocAvg += vocHistory[i];
  vocAvg /= VOC_WINDOW;

  // ------------------------------------------------
  // CO SENSOR (CORRECT ELECTROCHEMICAL MODEL)
  // ------------------------------------------------
  int16_t rawADC = ads.readADC_SingleEnded(0);
  float voltage = rawADC * ADS_LSB;

  float deltaV = voltage - CO_BASELINE_V0;
  if (deltaV < 0) deltaV = 0;

  float co_ppm = deltaV * 1000.0;   // 1 mV ≈ 1 ppm
  if (co_ppm > 200) co_ppm = 200;

  // IIR smoothing
  coFiltered = (1.0 - CO_ALPHA) * coFiltered + CO_ALPHA * co_ppm;

  // ------------------------------------------------
  // OUTPUT
  // ------------------------------------------------
  Serial.println("\n================ AIR QUALITY STATUS ================");

  Serial.print("🌡 Temperature (°C)     : ");
  Serial.println(bme.temperature, 2);

  Serial.print("💧 Humidity (%)         : ");
  Serial.println(bme.humidity, 2);

  Serial.print("⛰ Pressure (hPa)       : ");
  Serial.println(bme.pressure / 100.0, 2);

  Serial.print("🧪 VOC Resistance (kΩ) : ");
  Serial.println(vocAvg, 2);

  Serial.print("🧪 VOC Level           : ");
  Serial.println(vocLevel(vocAvg));

  Serial.print("🧪 VOC Trend           : ");
  Serial.println(vocTrend(vocAvg, lastVocAvg));

  String coLvl = coQuality(coFiltered);
  Serial.print("☁ CO (ppm)             : ");
  Serial.print(coFiltered, 1);
  Serial.print(" | ");
  Serial.print(coLvl);
  Serial.print(" | ");
  Serial.println(coRiskNote(coLvl));

  if (oxygenValue > 0) {
    Serial.print("🫁 Oxygen (%)          : ");
    Serial.println(oxygenValue, 1);
  }

  Serial.println("====================================================");

  lastVocAvg = vocAvg;
  delay(2000);
}
