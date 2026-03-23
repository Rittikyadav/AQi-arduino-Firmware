#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Credentials
const char* ssid = "Suncraft_01";
const char* password = "sun@2011";

// MQTT Broker
const char* mqtt_server = "213.190.4.113";
const char* topic = "test/no2";

// WiFi & MQTT Clients
WiFiClient espClient;
PubSubClient client(espClient);

// ===== NO2 Sensor Configuration =====
const int No2_SENSOR_PIN = 34;   // NO2 sensor connected to GPIO34 (ADC)
const float RL_No2 = 1.0;        // Load resistor in Mega Ohms
const float R0_No2 = 11.48;      // Baseline resistance in clean air (Mega Ohms)
const float A = 0.97514;         // Calibration constant
const float B = -1.32562;        // Calibration exponent
const float V_REF = 3.3;         // ESP32 ADC reference voltage

// ====================================

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_NO2_Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  Serial.println("NO2 Sensor PPM Calculation Started...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int rawValue = analogRead(No2_SENSOR_PIN);
  float voltage = (rawValue / 4095.0) * V_REF;

  // Sensor resistance calculation
  float Rs_No2 = ((V_REF - voltage) * (RL_No2 * 1e6)) / voltage;
  float ratio = Rs_No2 / (R0_No2 * 1e6);

  float ppm_No2 = A * pow(ratio, B);

  // Serial Output
  Serial.print("ADC: ");
  Serial.print(rawValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V | Rs: ");
  Serial.print(Rs_No2 / 1e6);
  Serial.print(" MΩ | NO2 PPM: ");
  Serial.println(ppm_No2);

  // MQTT Publish
  String payload = String(ppm_No2, 2);
  client.publish(topic, payload.c_str());

  delay(2000);
}
