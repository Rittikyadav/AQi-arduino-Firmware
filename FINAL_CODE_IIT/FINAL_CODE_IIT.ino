#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_ADS1X15.h>

/* =====================================================
   PIN CONFIGURATION
   ===================================================== */
#define SDA_PIN 21
#define SCL_PIN 22
#define RXD2    16
#define TXD2    17

/* =====================================================
   OBJECTS
   ===================================================== */
Adafruit_BME680 bme;
Adafruit_ADS1115 ads;

/* =====================================================
   ADS1115 CONFIG
   ===================================================== */
#define ADS_LSB  (0.125f / 1000.0f)   // GAIN_ONE: 0.125 mV per bit

/* =====================================================
   BASELINES (FROM YOUR CALIBRATION)
   ===================================================== */
#define CO_BASELINE_V0   1.024256f     // CO baseline (V)
#define NH3_BASELINE_V0  0.008784f     // NH3 baseline (V)
#define NO2_BASELINE_V0  0.007580f     // NO2 baseline (V)

#define VOC_R0           15.53f        // VOC baseline resistance (kΩ)

/* =====================================================
   FILTER SETTINGS
   ===================================================== */
#define VOC_WINDOW 5
#define ALPHA_CO   0.2f
#define ALPHA_NH3  0.2f
#define ALPHA_NO2  0.2f

float vocBuffer[VOC_WINDOW];
int vocIndex = 0;

float coFiltered  = 0;
float nh3Filtered = 0;
float no2Filtered = 0;

/* =====================================================
   OXYGEN SENSOR
   ===================================================== */
uint8_t o2Buf[12];
float oxygenValue = -1;

/* =====================================================
   HELPER FUNCTIONS
   ===================================================== */
String temperatureLabel(float t) {
  if (t < 18) return "Cold";
  if (t <= 30) return "Comfortable";
  return "Hot";
}

String humidityLabel(float h) {
  if (h < 30) return "Dry";
  if (h <= 60) return "Comfortable";
  return "Humid";
}

String pressureLabel(float p) {
  if (p < 1000) return "Low pressure";
  if (p <= 1025) return "Normal pressure";
  return "High pressure";
}

String vocLabel(float r) {
  if (r > 60) return "Good air quality";
  if (r > 30) return "Moderate pollution";
  return "Poor air quality";
}

String gasLevel(float ppm) {
  if (ppm < 5) return "Low";
  if (ppm < 20) return "Moderate";
  return "High";
}

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bme.begin(0x76)) {
    Serial.println("BME688 not detected");
    while (1);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not detected");
    while (1);
  }
  ads.setGain(GAIN_ONE);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("====================================================");
  Serial.println(" AIR QUALITY MONITORING SYSTEM");
  Serial.println(" IIT DEMONSTRATION BUILD");
  Serial.println("====================================================");
}

/* =====================================================
   LOOP
   ===================================================== */
void loop() {

  /* ---------------- OXYGEN SENSOR ---------------- */
  while (Serial2.available()) {
    for (int i = 0; i < 11; i++) o2Buf[i] = o2Buf[i + 1];
    o2Buf[11] = Serial2.read();

    if (o2Buf[0] == 0x16 && o2Buf[1] == 0x09 && o2Buf[2] == 0x01) {
      uint16_t raw = o2Buf[4] | (o2Buf[5] << 8);
      float o2 = raw / 10.0f;
      if (o2 >= 18 && o2 <= 25) oxygenValue = o2;
    }
  }

  /* ---------------- BME688 ---------------- */
  if (!bme.performReading()) return;

  float vocR = bme.gas_resistance / 1000.0f;
  vocBuffer[vocIndex++] = vocR;
  if (vocIndex >= VOC_WINDOW) vocIndex = 0;

  float vocAvg = 0;
  for (int i = 0; i < VOC_WINDOW; i++) vocAvg += vocBuffer[i];
  vocAvg /= VOC_WINDOW;

  /* ---------------- ADS1115 GAS SENSORS ---------------- */
  float vCO  = ads.readADC_SingleEnded(0) * ADS_LSB;
  float vNH3 = ads.readADC_SingleEnded(1) * ADS_LSB;
  float vNO2 = ads.readADC_SingleEnded(2) * ADS_LSB;

  float co_ppm  = (vCO  - CO_BASELINE_V0)  * 1000.0f;
  float nh3_ppm = (vNH3 - NH3_BASELINE_V0) * 1000.0f;
  float no2_ppm = (vNO2 - NO2_BASELINE_V0) * 1000.0f;

  if (co_ppm < 0)  co_ppm = 0;
  if (nh3_ppm < 0) nh3_ppm = 0;
  if (no2_ppm < 0) no2_ppm = 0;

  coFiltered  = (1 - ALPHA_CO)  * coFiltered  + ALPHA_CO  * co_ppm;
  nh3Filtered = (1 - ALPHA_NH3) * nh3Filtered + ALPHA_NH3 * nh3_ppm;
  no2Filtered = (1 - ALPHA_NO2) * no2Filtered + ALPHA_NO2 * no2_ppm;

  /* ---------------- OUTPUT ---------------- */
  Serial.println();
  Serial.println("---------------------------------------------------");
  Serial.println(" AIR QUALITY STATUS REPORT");
  Serial.println("---------------------------------------------------");

  Serial.print("Temperature        : ");
  Serial.print(bme.temperature, 2);
  Serial.print(" °C  (");
  Serial.print(temperatureLabel(bme.temperature));
  Serial.println(")");

  Serial.print("Humidity           : ");
  Serial.print(bme.humidity, 2);
  Serial.print(" %  (");
  Serial.print(humidityLabel(bme.humidity));
  Serial.println(")");

  Serial.print("Pressure           : ");
  Serial.print(bme.pressure / 100.0f, 2);
  Serial.print(" hPa  (");
  Serial.print(pressureLabel(bme.pressure / 100.0f));
  Serial.println(")");

  Serial.print("VOC Gas Resistance : ");
  Serial.print(vocAvg, 2);
  Serial.print(" kΩ  (");
  Serial.print(vocLabel(vocAvg));
  Serial.println(")");

  Serial.print("Carbon Monoxide    : ");
  Serial.print(coFiltered, 2);
  Serial.print(" ppm  (");
  Serial.print(gasLevel(coFiltered));
  Serial.println(")");

  Serial.print("Ammonia (NH3)      : ");
  Serial.print(nh3Filtered, 2);
  Serial.print(" ppm (relative)");

  Serial.println();

  Serial.print("Nitrogen Dioxide   : ");
  Serial.print(no2Filtered, 2);
  Serial.print(" ppm (relative)");

  Serial.println();

  if (oxygenValue > 0) {
    Serial.print("Oxygen Level       : ");
    Serial.print(oxygenValue, 1);
    Serial.println(" %  (Normal oxygen)");
  }

  Serial.println("---------------------------------------------------");

  delay(2000);
}
