#include <WiFi.h>

// 🔹 Replace with your WiFi credentials
const char* ssid = "Suncraft_01";
const char* password = "sun@2011";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================");
  Serial.println("ESP32 WiFi TEST STARTED");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;

    if (retry > 20) {
      Serial.println("\n❌ WiFi Connection Failed!");
      return;
    }
  }

  Serial.println("\n✅ WiFi Connected Successfully!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("🌐 Router (Gateway) IP: ");
  Serial.println(WiFi.gatewayIP());
  
}

void loop() {
  // Print WiFi strength every 5 seconds
  Serial.print("📶 Signal Strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  delay(5000);
}
