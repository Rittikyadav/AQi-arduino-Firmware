#include <WiFi.h>
#include <PubSubClient.h>

#define RXD2 16
#define TXD2 17

HardwareSerial pmSerial(2);

// ---------------- WIFI ----------------
const char* ssid = "Suncraft_01";
const char* password = "sun@2011";

// ---------------- MQTT ----------------
const char* mqtt_server = "213.190.4.113";
const int mqtt_port = 1883;
const char* topic = "aqqi/device1";

WiFiClient espClient;
PubSubClient client(espClient);

uint8_t buffer[32];

// ---------------- WIFI CONNECT ----------------
void setup_wifi() {
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}

// ---------------- MQTT CONNECT ----------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32_AQI")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ---------------- AQI CALCULATION ----------------
int calculateAQI(int pm25) {
  if (pm25 <= 30) return pm25 * 2;
  else if (pm25 <= 60) return pm25 * 2;
  else if (pm25 <= 90) return pm25 * 2;
  else return pm25 * 2;
}

String getCategory(int aqi) {
  if (aqi <= 50) return "Good";
  else if (aqi <= 100) return "Satisfactory";
  else if (aqi <= 200) return "Moderate";
  else if (aqi <= 300) return "Poor";
  else return "Severe";
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  pmSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("=== FINAL AQI SYSTEM START ===");
}

// ---------------- LOOP ----------------
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (pmSerial.available() >= 32) {

    if (pmSerial.read() == 0x42 && pmSerial.read() == 0x4D) {

      for (int i = 0; i < 30; i++) {
        buffer[i] = pmSerial.read();
      }

      int pm1  = (buffer[2] << 8) | buffer[3];
      int pm25 = (buffer[4] << 8) | buffer[5];
      int pm10 = (buffer[6] << 8) | buffer[7];

      int aqi = calculateAQI(pm25);
      String category = getCategory(aqi);

      Serial.println("---- PM DATA ----");
      Serial.print("PM1: "); Serial.print(pm1); Serial.println(" µg/m³");
      Serial.print("PM2.5: "); Serial.print(pm25); Serial.println(" µg/m³");
      Serial.print("PM10: "); Serial.print(pm10); Serial.println(" µg/m³");

      // ----------- MQTT PAYLOAD -----------
      String payload = "{";

      payload += "\"device\":\"aqii_device_1\",";

      // Numeric values
      payload += "\"pm1\":" + String(pm1) + ",";
      payload += "\"pm25\":" + String(pm25) + ",";
      payload += "\"pm10\":" + String(pm10) + ",";

      // With unit (your requirement)
      payload += "\"pm1_str\":\"" + String(pm1) + " µg/m³\",";
      payload += "\"pm25_str\":\"" + String(pm25) + " µg/m³\",";
      payload += "\"pm10_str\":\"" + String(pm10) + " µg/m³\",";

      payload += "\"aqi\":" + String(aqi) + ",";
      payload += "\"category\":\"" + category + "\"";

      payload += "}";

      client.publish(topic, payload.c_str());

      Serial.print("Published: ");
      Serial.println(payload);

      delay(2000);
    }
  }
}
