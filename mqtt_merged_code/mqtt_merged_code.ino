#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_ADS1X15.h>
#include <time.h>

// =====================
// DEVICE INFO
// =====================
#define DEVICE_ID "DEVICE_001"

// =====================
// WIFI & MQTT CONFIG
// =====================
const char* WIFI_SSID = "Suncraft_01";
const char* WIFI_PASS = "sun@2011";

const char* MQTT_BROKER = "213.190.4.113";
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "AQM/Device-1";

// =====================
// MQTT INTERVAL (10 min)
// =====================
const unsigned long PUBLISH_INTERVAL = 60000;
unsigned long lastPublishTime = 0;

// =====================
// PIN CONFIG
// =====================
#define SDA_PIN 21
#define SCL_PIN 22
#define RXD2 16
#define TXD2 17

// =====================
// OBJECTS
// =====================
WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_BME680 bme;
Adafruit_ADS1115 ads;

// =====================
// CO (SEN0564) CONFIG
// =====================
const float CO_BASELINE_V0 = 0.3965;      // Clean-air baseline
const float ADS_LSB = 0.125 / 1000.0;     // ADS1115 GAIN_ONE

// =====================
// VOC FILTER
// =====================
float vocHistory[5] = {0};
int vocIndex = 0;
float lastVocAvg = 0;

// =====================
// OXYGEN UART
// =====================
uint8_t o2Buf[12];
float oxygenValue = -1;

// =====================
// QUALITY FUNCTIONS
// =====================
String coQuality(float ppm) {
  if (ppm <= 9) return "EXCELLENT";
  if (ppm <= 35) return "ACCEPTABLE";
  if (ppm <= 100) return "WARNING";
  if (ppm <= 300) return "DANGEROUS";
  return "CRITICAL";
}

String vocLevel(float r) {
  if (r > 50) return "CLEAN";
  if (r > 30) return "GOOD";
  if (r > 15) return "MODERATE";
  return "POOR";
}

String vocTrend(float cur, float prev) {
  if (cur > prev + 1.0) return "IMPROVING";
  if (cur < prev - 1.0) return "DEGRADING";
  return "STABLE";
}

// =====================
// WIFI CONNECT
// =====================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
}

// =====================
// TIME SETUP
// =====================
void setupTime() {
  configTime(0, 0, "pool.ntp.org");
  Serial.print("Syncing time");
  time_t now;
  while ((now = time(nullptr)) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" DONE");
}

// =====================
// MQTT CONNECT
// =====================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println(" CONNECTED");
    } else {
      Serial.print(" FAILED rc=");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  connectWiFi();
  setupTime();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  // ---------- BME688 ----------
  if (!bme.begin(0x76)) {
    Serial.println("❌ BME688 not found");
    while (1);
  }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setGasHeater(320, 150);

  // ---------- ADS1115 ----------
  if (!ads.begin(0x48)) {
    Serial.println("❌ ADS1115 not found");
    while (1);
  }
  ads.setGain(GAIN_ONE);

  // ---------- Oxygen ----------
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\nAIR QUALITY MONITOR READY");
  Serial.println("---------------------------------------------");
}

// =====================
// LOOP
// =====================
void loop() {

  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  // ---------- Oxygen (continuous read) ----------
  while (Serial2.available()) {
    for (int i = 0; i < 11; i++) o2Buf[i] = o2Buf[i + 1];
    o2Buf[11] = Serial2.read();

    if (o2Buf[0] == 0x16 && o2Buf[1] == 0x09 && o2Buf[2] == 0x01) {
      float o2 = ((o2Buf[5] << 8) | o2Buf[4]) / 10.0;
      if (o2 >= 18.0 && o2 <= 25.0) oxygenValue = o2;
    }
  }

  // ---------- 10 MIN PUBLISH ----------
  unsigned long nowMillis = millis();

  if (nowMillis - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = nowMillis;

    // BME688
    bme.performReading();
    float vocR = bme.gas_resistance / 1000.0;

    vocHistory[vocIndex++] = vocR;
    if (vocIndex >= 5) vocIndex = 0;

    float vocAvg = 0;
    for (int i = 0; i < 5; i++) vocAvg += vocHistory[i];
    vocAvg /= 5.0;

    // CO
    float voltage = ads.readADC_SingleEnded(0) * ADS_LSB;
    float ratio = voltage / CO_BASELINE_V0;
    float co_ppm = 100.0 * pow(ratio, -1.5);

    // Timestamp
    time_t now = time(nullptr);

    // JSON
    String payload = "{";
    payload += "\"device_id\":\"" DEVICE_ID "\",";
    payload += "\"timestamp\":" + String((long)now) + ",";
    payload += "\"temperature\":" + String(bme.temperature, 2) + ",";
    payload += "\"humidity\":" + String(bme.humidity, 2) + ",";
    payload += "\"pressure\":" + String(bme.pressure / 100.0, 2) + ",";
    payload += "\"voc_kohm\":" + String(vocAvg, 2) + ",";
    payload += "\"voc_level\":\"" + vocLevel(vocAvg) + "\",";
    payload += "\"voc_trend\":\"" + vocTrend(vocAvg, lastVocAvg) + "\",";
    payload += "\"co_ppm\":" + String(co_ppm, 1) + ",";
    payload += "\"co_level\":\"" + coQuality(co_ppm) + "\",";
    payload += "\"oxygen\":" + String(oxygenValue, 1);
    payload += "}";

    mqtt.publish(MQTT_TOPIC, payload.c_str());

    Serial.println("📤 MQTT DATA SENT (10 min)");
    Serial.println(payload);
    Serial.println("---------------------------------------------");

    lastVocAvg = vocAvg;
  }
}
