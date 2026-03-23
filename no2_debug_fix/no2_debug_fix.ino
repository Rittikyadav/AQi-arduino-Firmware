#include <WiFi.h>
#include <PubSubClient.h>

// ================= WiFi & MQTT =================
const char* ssid = "Suncraft_01";
const char* password = "sun@2011";
const char* mqtt_server = "213.190.4.113";
const char* topic = "test/no2";

WiFiClient espClient;
PubSubClient client(espClient);

// ================= NO2 SENSOR ==================
const int No2_SENSOR_PIN = 34;     // ADC1 pin (GOOD choice)
const float RL_No2 = 1.0;          // Load resistor in MΩ (change to 0.47 later)
const float R0_No2 = 11.48;         // Baseline resistance in clean air (MΩ)
const float A = 0.97514;            // Calibration constant
const float B = -1.32562;           // Calibration exponent
const float V_REF = 3.3;            // ESP32 reference voltage

// ================= SETTINGS ====================
#define ADC_SAMPLES 50
#define MIN_VOLTAGE 0.15            // ADC noise floor clamp

// =================================================

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_NO2_CLIENT")) {
      Serial.println("connected");
    } else {
      Serial.print("failed (");
      Serial.print(client.state());
      Serial.println("), retrying...");
      delay(3000);
    }
  }
}

// ---------- Stable ADC Reading ----------
float readStableADC() {
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(No2_SENSOR_PIN);
    delay(2);
  }
  return (float)sum / ADC_SAMPLES;
}

void setup() {
  Serial.begin(115200);

  // ADC tuning (VERY IMPORTANT)
  analogSetPinAttenuation(No2_SENSOR_PIN, ADC_11db);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  Serial.println("\nNO2 Sensor Stabilized Measurement Started");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // ---- Read ADC ----
  float rawADC = readStableADC();
  float voltage = (rawADC / 4095.0) * V_REF;

  // Clamp low voltage noise
  if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;

  // ---- Sensor math ----
  float Rs = ((V_REF - voltage) * (RL_No2 * 1e6)) / voltage; // Ohms
  float ratio = Rs / (R0_No2 * 1e6);
  float ppm = A * pow(ratio, B);

  // Prevent negative or NaN ppm
  if (ppm < 0 || isnan(ppm)) ppm = 0;

  // ---- Serial Output ----
  Serial.print("ADC: "); Serial.print(rawADC, 0);
  Serial.print(" | V: "); Serial.print(voltage, 3);
  Serial.print(" V | Rs: "); Serial.print(Rs / 1e6, 2);
  Serial.print(" MΩ | NO2: ");
  Serial.print(ppm, 2);
  Serial.println(" ppm");

  // ---- MQTT Publish ----
  char payload[16];
  dtostrf(ppm, 6, 2, payload);
  client.publish(topic, payload);

  delay(2000);
}
