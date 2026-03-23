#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_ADS1X15.h>

/* =====================================================
   WIFI & MQTT CONFIG
   ===================================================== */
#define WIFI_SSID     "Suncraft_01"
#define WIFI_PASS     "sun@2011"

#define MQTT_SERVER   "213.190.4.113"
#define MQTT_PORT     1883
#define MQTT_TOPIC    "aqi/device1"
#define DEVICE_ID     "device1"

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
WiFiClient espClient;
PubSubClient mqttClient(espClient);

Adafruit_BME680 bme;
Adafruit_ADS1115 ads;

/* =====================================================
   BASELINES (INITIAL)
   ===================================================== */
float coBaseline  = 0.060000;
float nh3Baseline = 0.0815;
float no2Baseline = 0.0815;

/* =====================================================
   FILTER SETTINGS
   ===================================================== */
#define VOC_WINDOW 5
#define ALPHA      0.2f

float vocBuf[VOC_WINDOW];
int vocIndex = 0;

float coF = 0, nh3F = 0, no2F = 0;

/* =====================================================
   DRIFT CONTROL
   ===================================================== */
const float gasHysteresis = 0.005;
const float gasAlpha = 0.001;

/* =====================================================
   VOC BASELINE
   ===================================================== */
float vocBaseline = 15.53;

/* =====================================================
   OXYGEN SENSOR
   ===================================================== */
uint8_t o2Buf[12];
float oxygenValue = -1;

/* =====================================================
   TIMING
   ===================================================== */
unsigned long lastPublish = 0;
const unsigned long PUBLISH_INTERVAL = 60000;

/* =====================================================
   WIFI & MQTT FUNCTIONS
   ===================================================== */
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(DEVICE_ID)) {
      Serial.println("MQTT connected");
    } else {
      delay(2000);
    }
  }
}

/* =====================================================
   SETUP
   ===================================================== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  connectWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

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

  ads.setGain(GAIN_ONE);   // KEEP SAME (NO BREAK)

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("AIR QUALITY MONITOR SYSTEM INITIALIZED");
}

/* =====================================================
   LOOP
   ===================================================== */
void loop() {

  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  /* -------- Oxygen -------- */
  while (Serial2.available()) {
    for (int i = 0; i < 11; i++) o2Buf[i] = o2Buf[i + 1];
    o2Buf[11] = Serial2.read();

    if (o2Buf[0] == 0x16 && o2Buf[1] == 0x09 && o2Buf[2] == 0x01) {
      uint16_t raw = o2Buf[4] | (o2Buf[5] << 8);
      float o2 = raw / 10.0f;
      if (o2 >= 18 && o2 <= 25) oxygenValue = o2;
    }
  }

  /* -------- BME688 -------- */
  if (!bme.performReading()) return;

  float vocR = bme.gas_resistance / 1000.0f;
  vocBuf[vocIndex++] = vocR;
  if (vocIndex >= VOC_WINDOW) vocIndex = 0;

  float vocAvg = 0;
  for (int i = 0; i < VOC_WINDOW; i++) vocAvg += vocBuf[i];
  vocAvg /= VOC_WINDOW;

  /* -------- VOC BASELINE TRACKING -------- */
  float vocDelta = (vocAvg - vocBaseline) / vocBaseline;

  if (abs(vocDelta) < 0.05) {
    vocBaseline = vocBaseline * 0.999 + vocAvg * 0.001;
  }

  /* -------- GAS SENSORS (FIXED) -------- */
  float vCO  = ads.computeVolts(ads.readADC_SingleEnded(0));
  float vNH3 = ads.computeVolts(ads.readADC_SingleEnded(1));
  float vNO2 = ads.computeVolts(ads.readADC_SingleEnded(2));

  // CO
  float deltaCO = vCO - coBaseline;
  if (abs(deltaCO) < gasHysteresis) {
    coBaseline = coBaseline * (1.0 - gasAlpha) + vCO * gasAlpha;
  }
  float coPPM = deltaCO * 1000.0f;
  if (coPPM < 0) coPPM = 0;

  // NH3
  float deltaNH3 = vNH3 - nh3Baseline;
  if (abs(deltaNH3) < gasHysteresis) {
    nh3Baseline = nh3Baseline * (1.0 - gasAlpha) + vNH3 * gasAlpha;
  }
  float nh3PPM = deltaNH3 * 1000.0f;
  if (nh3PPM < 0) nh3PPM = 0;

  // NO2
  float deltaNO2 = vNO2 - no2Baseline;
  if (abs(deltaNO2) < gasHysteresis) {
    no2Baseline = no2Baseline * (1.0 - gasAlpha) + vNO2 * gasAlpha;
  }
  float no2PPM = deltaNO2 * 1000.0f;
  if (no2PPM < 0) no2PPM = 0;

  /* -------- FILTER -------- */
  coF  = (1 - ALPHA) * coF  + ALPHA * coPPM;
  nh3F = (1 - ALPHA) * nh3F + ALPHA * nh3PPM;
  no2F = (1 - ALPHA) * no2F + ALPHA * no2PPM;

  /* -------- MQTT -------- */
  if (millis() - lastPublish > PUBLISH_INTERVAL) {
    lastPublish = millis();

    char payload[512];
    snprintf(payload, sizeof(payload),
      "{"
      "\"device_id\":\"%s\","
      "\"temperature\":%.2f,"
      "\"humidity\":%.2f,"
      "\"pressure\":%.2f,"
      "\"voc_kohm\":%.2f,"
      "\"co_ppm\":%.2f,"
      "\"nh3_ppm\":%.2f,"
      "\"no2_ppm\":%.2f,"
      "\"oxygen\":%.1f"
      "}",
      DEVICE_ID,
      bme.temperature,
      bme.humidity,
      bme.pressure / 100.0f,
      vocAvg,
      coF,
      nh3F,
      no2F,
      oxygenValue
    );

    mqttClient.publish(MQTT_TOPIC, payload);
    Serial.println("Data published to MQTT");
  }

  delay(2000);
}
