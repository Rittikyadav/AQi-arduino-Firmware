#include <WiFi.h>
#include <PubSubClient.h>
#include <PMS.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include "time.h"

// --------------------- USER CONFIG ---------------------
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASS";

// MQTT
const char* MQTT_BROKER = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_DEVICE_ID = "aqi_device_01";
const char* MQTT_TOPIC_STATUS = "aqi/device01/status";
const char* MQTT_TOPIC_DATA   = "aqi/device01/data";
const char* MQTT_WILL_TOPIC   = "aqi/device01/lastwill";
const char* HTTP_ENDPOINT = "http://api.plumuleresearch.com/api/create.php"; // optional

// Intervals
const uint32_t MINUTE_INTERVAL = 60UL * 1000UL;      // 1 minute
const uint32_t SIX_HOUR_INTERVAL = 6UL * 60UL * 60UL * 1000UL; // 6 hours
const uint32_t DAILY_REBOOT_INTERVAL = 24UL * 60UL * 60UL * 1000UL; // daily reboot

// Pins
#define DHT_PIN        2     // DHT11 data
#define DHT_TYPE       DHT11
#define PMS_RX         16
#define PMS_TX         17
#define MQ7_PIN        34    // analog (AOUT)
#define MQ7_HEATER_PIN 27    // digital to switch heater (via MOSFET)
#define MQ3_PIN        33
#define CO2_PIN        35
#define O2_PIN         36
#define FAN_PIN        14

// ADC / Sampling
const int ADC_SAMPLES = 7;  // odd number for median filter
const int ADC_AVG = 5;      // average after median smoothing

// MQ7 Heater cycle (example timings - adjust per datasheet)
const uint32_t MQ7_HEAT_HIGH_MS = 60000UL;  // high heater (preheat) (60s)
const uint32_t MQ7_HEAT_LOW_MS  = 90000UL;  // low heater (measure) (90s)
const uint32_t MQ7_READ_DELAY_MS = 3000UL;  // wait after switching to low before reading

// Calibration constants - **REPLACE WITH YOUR CALIBRATED VALUES**
const float MQ7_RL = 10000.0f;          // RL in ohms
float MQ7_R0 = 58000.0f;                // calibrate in fresh air
const float MQ7_A = 9.478f;             // curve constant a (example)
const float MQ7_B = -1.347f;            // curve exponent b (example)

float MQ3_R0 = 20000.0f;                // calibrate MQ3 R0
const float MQ3_A = 0.8f;
const float MQ3_B = -1.4f;

const float CO2_V0 = 0.40f;             // voltage at 400ppm measure and set
const float CO2_SLOPE = 0.001f;         // V per ppm (example) -> REPLACE
const float O2_V0 = 2.09f;              // voltage at 20.9% (example) -> REPLACE

// --------------------- GLOBALS ---------------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);
PMS pms(Serial1);
PMS::DATA pmsData;
DHT dht(DHT_PIN, DHT_TYPE);

// Buffer for samples (360 = 6h * 60)
#define MAX_SAMPLES 360
struct Sample {
  float temp, hum;
  float pm2_5, pm10;
  float co, co2, voc, o2;
  uint32_t ts;
};
Sample samples[MAX_SAMPLES];
int sampleCount = 0;
int sampleWriteIndex = 0;

// Timers
unsigned long lastMinuteMillis = 0;
unsigned long lastSixHourMillis = 0;
unsigned long startMillis = 0;
unsigned long lastMQ7HeaterSwitch = 0;
bool mq7HeaterHigh = true; // start with high heater period
unsigned long lastDailyReboot = 0;

// Watchdog
#define WDT_TIMEOUT_S 20  // seconds
// --------------------- UTIL FUNCTIONS ---------------------

// median of array (odd n)
float medianFilter(int pin) {
  int vals[ADC_SAMPLES];
  for (int i = 0; i < ADC_SAMPLES; ++i) {
    vals[i] = analogRead(pin);
    delay(10);
  }
  // simple insertion sort
  for (int i = 1; i < ADC_SAMPLES; ++i) {
    int key = vals[i], j = i - 1;
    while (j >= 0 && vals[j] > key) {
      vals[j + 1] = vals[j];
      j--;
    }
    vals[j + 1] = key;
  }
  return vals[ADC_SAMPLES / 2];
}

float smoothADC(int pin) {
  // median then average of small window
  long sum = 0;
  for (int i = 0; i < ADC_AVG; ++i) {
    sum += medianFilter(pin);
  }
  return (float)sum / ADC_AVG;
}

float adcToVoltage(float adcValue) {
  // ESP32 ADC default reference -> 0..4095 -> map to 0..3.3 (or 0..5 if using external divider)
  // If your sensors output up to 5V, use a proper divider and adjust here.
  // We assume sensors' analog line scaled to 0..3.3V
  return adcValue * (3.3f / 4095.0f);
}

// MQ7 calculation (Rs -> ppm)  -- read only during low-heater stable window
float calcMQ7ppm(float adcVal) {
  float Vout = adcToVoltage(adcVal);
  if (Vout <= 0.01f) return 0.0f;
  float Rs = (3.3f - Vout) / Vout * MQ7_RL; // if divider used, adjust RL or scaling
  float ratio = Rs / MQ7_R0;
  float ppm = MQ7_A * pow(ratio, MQ7_B);
  return (ppm > 0.0f ? ppm : 0.0f);
}

float calcMQ3ppm(float adcVal) {
  float Vout = adcToVoltage(adcVal);
  if (Vout <= 0.01f) return 0.0f;
  float Rs = (3.3f - Vout) / Vout * MQ7_RL; // using same RL
  float ratio = Rs / MQ3_R0;
  float ppm = MQ3_A * pow(ratio, MQ3_B);
  return (ppm > 0.0f ? ppm : 0.0f);
}

float calcCO2ppm(float adcVal) {
  float Vout = adcToVoltage(adcVal);
  float ppm = (Vout - CO2_V0) / CO2_SLOPE + 400.0f;
  if (ppm < 0.0f) ppm = 0.0f;
  return ppm;
}

float calcO2pct(float adcVal) {
  float Vout = adcToVoltage(adcVal);
  float pct = (Vout / O2_V0) * 20.9f;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

// Add a sample to circular buffer
void pushSample(const Sample &s) {
  samples[sampleWriteIndex] = s;
  sampleWriteIndex = (sampleWriteIndex + 1) % MAX_SAMPLES;
  if (sampleCount < MAX_SAMPLES) sampleCount++;
}

// Compute aggregated stats (mean) and return JSON string in char buffer
void publish6HourAggregate() {
  if (sampleCount == 0) return;

  double st=0, sh=0, sp2=0, sp10=0, sc=0, sco2=0, svoc=0, so=0;
  for (int i = 0; i < sampleCount; ++i) {
    st += samples[i].temp;
    sh += samples[i].hum;
    sp2 += samples[i].pm2_5;
    sp10 += samples[i].pm10;
    sc += samples[i].co;
    sco2 += samples[i].co2;
    svoc += samples[i].voc;
    so += samples[i].o2;
  }
  double cnt = sampleCount;
  StaticJsonDocument<512> doc;
  doc["device_id"] = MQTT_DEVICE_ID;
  doc["samples"] = (int)sampleCount;
  doc["temp"] = st / cnt;
  doc["hum"] = sh / cnt;
  doc["pm2_5"] = sp2 / cnt;
  doc["pm10"] = sp10 / cnt;
  doc["co"] = sc / cnt;
  doc["co2"] = sco2 / cnt;
  doc["voc"] = svoc / cnt;
  doc["o2"] = so / cnt;
  doc["rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  // time
  time_t now;
  time(&now);
  doc["ts"] = now;

  char outBuf[700];
  size_t n = serializeJson(doc, outBuf, sizeof(outBuf));
  // Publish to MQTT
  mqttClient.publish(MQTT_TOPIC_DATA, outBuf, n, false);
  Serial.println("Published 6-hour aggregate to MQTT:");
  Serial.println(outBuf);

  // Optional: HTTP POST (uncomment if needed)
  /*
  HTTPClient http;
  http.begin(HTTP_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(outBuf);
  if (code > 0) {
    Serial.printf("HTTP POST code: %d\n", code);
    String resp = http.getString();
    Serial.println(resp);
  } else {
    Serial.printf("HTTP POST failed, error: %d\n", code);
  }
  http.end();
  */

  // reset buffer
  sampleCount = 0;
  sampleWriteIndex = 0;
}

// Publish a lightweight status every minute
void publishStatus(float temp, float hum, float pm2_5) {
  StaticJsonDocument<256> doc;
  doc["device_id"] = MQTT_DEVICE_ID;
  time_t now; time(&now);
  doc["ts"] = now;
  doc["temp"] = temp;
  doc["hum"] = hum;
  doc["pm2_5"] = pm2_5;
  doc["rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  char buf[300];
  size_t n = serializeJson(doc, buf);
  mqttClient.publish(MQTT_TOPIC_STATUS, buf, n, true); // retained status
}

// MQTT connect with backoff
void mqttConnect() {
  uint32_t backoff = 1000;
  const uint32_t maxBackoff = 30000;
  while (!mqttClient.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected, waiting...");
      delay(1000);
      continue;
    }
    Serial.println("Attempting MQTT connect...");
    // set LWT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(nullptr);
    if (mqttClient.connect(MQTT_DEVICE_ID, nullptr, nullptr, MQTT_WILL_TOPIC, 0, true, "offline")) {
      Serial.println("MQTT connected");
      mqttClient.publish(MQTT_WILL_TOPIC, "online", true);
      break;
    } else {
      Serial.printf("MQTT connect failed, rc=%d. Backing off %u ms\n", mqttClient.state(), backoff);
      delay(backoff);
      backoff = min(backoff * 2, maxBackoff);
    }
  }
}

// Setup NTP time using system timezone UTC by default (adjust)
void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  uint32_t tries = 0;
  while (now < 1600000000UL && tries < 20) { // wait for valid time
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    tries++;
  }
  Serial.println();
  if (now < 1600000000UL) Serial.println("NTP sync failed, continue with device uptime.");
  else Serial.println("NTP synced.");
}

// --------------------- SETUP ---------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Starting ESP32 AQI device");

  // Setup WDT
  esp_task_wdt_init(WDT_TIMEOUT_S, true); // enable panic so ESP restarts on WDT
  esp_task_wdt_add(NULL); // add current task (loop) to WDT

  // Initialize hardware
  pinMode(FAN_PIN, OUTPUT); digitalWrite(FAN_PIN, LOW);
  pinMode(MQ7_HEATER_PIN, OUTPUT); digitalWrite(MQ7_HEATER_PIN, HIGH); // start with heater HIGH
  pinMode(MQ7_PIN, INPUT);
  pinMode(MQ3_PIN, INPUT);
  pinMode(CO2_PIN, INPUT);
  pinMode(O2_PIN, INPUT);

  dht.begin();
  Serial1.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pms.passiveMode();

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("\nWiFi connected.");
  else Serial.println("\nWiFi not connected at setup (will retry later).");

  // Setup time & mqtt
  setupTime();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  startMillis = millis();
  lastMinuteMillis = millis();
  lastSixHourMillis = millis();
  lastDailyReboot = millis();
  lastMQ7HeaterSwitch = millis();
  mq7HeaterHigh = true; // start high phase
}

// --------------------- LOOP ---------------------
void loop() {
  // feed watchdog
  esp_task_wdt_reset();

  // MQTT ensure connected
  if (!mqttClient.connected()) mqttConnect();
  mqttClient.loop();

  unsigned long nowMs = millis();

  // MQ7 heater state machine
  // If we're in high phase, stay high for MQ7_HEAT_HIGH_MS then switch to low.
  if (mq7HeaterHigh && (nowMs - lastMQ7HeaterSwitch >= MQ7_HEAT_HIGH_MS)) {
    // switch to low (measurement) phase
    digitalWrite(MQ7_HEATER_PIN, LOW); // low heater voltage via appropriate circuit
    mq7HeaterHigh = false;
    lastMQ7HeaterSwitch = nowMs;
    Serial.println("MQ7: switched to LOW (measurement) heater phase");
  } else if (!mq7HeaterHigh && (nowMs - lastMQ7HeaterSwitch >= MQ7_HEAT_LOW_MS)) {
    // switch back to high
    digitalWrite(MQ7_HEATER_PIN, HIGH);
    mq7HeaterHigh = true;
    lastMQ7HeaterSwitch = nowMs;
    Serial.println("MQ7: switched to HIGH (preheat) heater phase");
  }

  // 1-minute sampling - status and push into buffer
  if (nowMs - lastMinuteMillis >= MINUTE_INTERVAL) {
    lastMinuteMillis = nowMs;

    // Turn fan on briefly
    digitalWrite(FAN_PIN, HIGH);
    delay(5000);
    digitalWrite(FAN_PIN, LOW);

    // Read DHT
    float temp = NAN, hum = NAN;
    temp = dht.readTemperature();
    hum = dht.readHumidity();

    // Read PM sensor
    float pm2_5 = 0, pm10 = 0;
    pms.wakeUp();
    delay(1500);
    pms.requestRead();
    if (pms.readUntil(pmsData)) {
      pm2_5 = pmsData.PM_AE_UG_2_5;
      pm10  = pmsData.PM_AE_UG_10_0;
    }
    pms.sleep();

    // Read analog sensors
    // For MQ7, only read during LOW heater phase and after a small stabilization delay
    float co_ppm = 0.0f;
    if (!mq7HeaterHigh) {
      // ensure we've been in low for long enough
      if (millis() - lastMQ7HeaterSwitch >= MQ7_READ_DELAY_MS) {
        float raw = smoothADC(MQ7_PIN);
        co_ppm = calcMQ7ppm(raw);
      } else {
        // not yet stable - try median read as placeholder
        float raw = medianFilter(MQ7_PIN);
        co_ppm = calcMQ7ppm(raw);
      }
    } else {
      // during high phase, reading is not valid
      co_ppm = NAN;
    }

    float raw_voc = smoothADC(MQ3_PIN);
    float voc_ppm = calcMQ3ppm(raw_voc);

    float raw_co2 = smoothADC(CO2_PIN);
    float co2_ppm = calcCO2ppm(raw_co2);

    float raw_o2 = smoothADC(O2_PIN);
    float o2_pct = calcO2pct(raw_o2);

    // Create sample
    Sample s;
    s.temp = temp; s.hum = hum;
    s.pm2_5 = pm2_5; s.pm10 = pm10;
    s.co = co_ppm; s.co2 = co2_ppm; s.voc = voc_ppm; s.o2 = o2_pct;
    s.ts = (uint32_t)time(nullptr);
    pushSample(s);

    // Publish lightweight status (retained)
    publishStatus(temp, hum, pm2_5);

    Serial.printf("Minute sample: T=%.1f H=%.1f PM2.5=%.1f CO=%.2f CO2=%.1f VOC=%.2f O2=%.2f\n",
                  temp, hum, pm2_5, co_ppm, co2_ppm, voc_ppm, o2_pct);

    // safety: if WiFi disconnected try reconnect now
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost - attempting reconnect...");
      WiFi.reconnect();
    }
  }

  // 6-hour aggregated publish
  if (nowMs - lastSixHourMillis >= SIX_HOUR_INTERVAL) {
    lastSixHourMillis = nowMs;
    publish6HourAggregate();
  }

  // Daily soft reboot to avoid long-run fragmentation/hangs
  if (nowMs - lastDailyReboot >= DAILY_REBOOT_INTERVAL) {
    Serial.println("Daily reboot triggered.");
    delay(100);
    ESP.restart();
  }

  delay(10); // small yield
}
