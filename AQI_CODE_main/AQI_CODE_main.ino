
#include <WiFi.h>
#include <HTTPClient.h>
#include "PMS.h"
#include "DHT11.h"
#include "CO2Sensor.h"
#include <math.h>

// WiFi parameters
const char* ssid = "wifi name";
const char* password = "wifi password";
const char* serverName = "";
=
// Define constants
const float VCC = 5.0; // Supply voltage
const float R_L = 10000.0; // Load resistor (10 kΩ)
const float R0 = 92000.0; // Baseline resistance (R0) in clean air, adjust based on your calibration
const float a = 0.893; // Constant 'a' from the sensor's characteristic curve, example value
const float b = -0.504; // Constant 'b' from the sensor's characteristic curve
const float Vmin = 0.265; // Minimum output voltage (mV)
const float Vmax = 0.325; // Maximum output voltage (mV)
const float ppmMin = 350.0; // Minimum ppm concentration
const float ppmMax = 3500.0; // Maximum ppm concentration
const float m=100.0;
const float n=0.0;

// Sensor pins
const int MQ7pin = 34;  // AOUT of MQ7 connected to pin 34 (Analog capable)
CO2Sensor co2Sensor(35, 0.99, 100);  // AOUT of CO2 sensor connected to pin 35 (Analog capable)
const int DHTpin = 2;   // Digital pin for DHT11
//const int sensorPin = 32; // Analog pin connected to the NH3 sensor
const int VOCpin = 33;  // Analog pin connected to the VOC sensor

PMS pms(Serial1);
PMS::DATA data;
DHT11 dht11(DHTpin);

WiFiClient wifiClient;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);  // Serial1 to PMS sensor
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("R");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  co2Sensor.calibrate();
  pms.passiveMode();  // Initialize PMS sensor in passive mode
}

void loop() {
  // Read and send PM sensor data
  pms.wakeUp();
  delay(1500);  // Warm up time for sensor stabilization

  pms.requestRead();
  if (pms.readUntil(data)) {
    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);
    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);
  } else {
    Serial.println("No PM data.");
  }
  pms.sleep();

  // Read DHT11 sensor data
  int temperature = dht11.readTemperature();
  int humidity = dht11.readHumidity();
  if (temperature != DHT11::ERROR_CHECKSUM && temperature != DHT11::ERROR_TIMEOUT &&
      humidity != DHT11::ERROR_CHECKSUM && humidity != DHT11::ERROR_TIMEOUT) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  // Read CO and CO2 concentrations
  float CO_concentration = calculateCO(analogRead(MQ7pin));
  float CO2_value = co2Sensor.read();
  //int sensorValue = analogRead(sensorPin);
  float VOC_concentration = calculateVOC(analogRead(VOCpin));
 // float ammoniaConcentration = map(sensorValue, 0, 1023, 0, 20);

  // Send all data to server
  HTTPClient http;
  http.begin(wifiClient, serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "pm2_5=" + String(data.PM_AE_UG_2_5) + "&pm10=" + String(data.PM_AE_UG_10_0) +
                    "&temp=" + String(temperature) + "&hum=" + String(humidity) +
                    "&co=" + String(CO_concentration) + "&co2=" + String(CO2_value) + "&NH3=" + String(VOC_concentration); //"&NH3=" + String(ammoniaConcentration)
  int httpResponseCode = http.POST(postData);
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server Response: " + response);
  } else {
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  Serial.print("CO Concentration: ");
  Serial.print(CO_concentration);
  Serial.println(" ppm");
  Serial.print("CO2 Concentration: ");
  Serial.print(CO2_value);
  Serial.println(" ppm");
  Serial.print("VOC Concentration: ");
  Serial.print(VOC_concentration);
  Serial.println(" ppm");
  //Serial.print("NH3 Concentration: ");
  //Serial.print(ammoniaConcentration);
  //Serial.println(" ppm");

  delay(1500);  // Delay before the next loop iteration
}

float calculateCO(float CO_concentration) {
  // Convert the analog readings to voltage
  float voltage = CO_concentration * (VCC / 4095.0);

  // Calculate the sensor resistance
  float sensorResistance = (VCC - voltage) / voltage * R0;

  // Calculate the resistance ratio (R_S / R_L)
  float resistanceRatio = sensorResistance / R_L;

  // Calculate ppm based on the resistance ratio
   float ppm3 = a * pow((resistanceRatio), b);

  return ppm3;
}

float calculateCO2(float CO2_value) {
  // Convert the analog readings to voltage
  float co2voltage = CO2_value * (VCC / 4095.0);

  // Hypothetical calculation for CO2 concentration
  float ppm1 = pow(10, ((log10(co2voltage) - log10(Vmin)) / (log10(Vmax) - log10(Vmin)) * (log10(ppmMax) - log10(ppmMin)) + log10(ppmMin))); // Placeholder formula for CO2 concentration
  ppm1=abs(ppm1);
  return ppm1;
}
float calculateVOC(float VOC_concentration) {
  // Convert the analog readings to voltage
  float vocvoltage = VOC_concentration * (3.3 / 4095.0);

  // Hypothetical calculation for VOC concentration
  // You need to replace this with the actual formula based on your sensor's data sheet
  float ppm2 = m * vocvoltage + n; ; // Placeholder formula for VOC concentration
  return ppm2;
}
