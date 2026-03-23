#include <Wire.h>
#include <SensirionI2cScd30.h>

SensirionI2cScd30 scd30;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin(21, 22);

  Serial.println("Initializing SCD30...");

  scd30.begin(Wire, 0x61);

  uint16_t error;
  char errorMessage[256];

  // Stop previous measurement
  error = scd30.stopPeriodicMeasurement();
  delay(500);

  // Start measurement
  error = scd30.startPeriodicMeasurement(0);

  if (error) {
    Serial.print("Error starting measurement: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.println("✅ SCD30 Started!");
  }
}

void loop() {
  uint16_t error;
  char errorMessage[256];

  // ✅ FIXED TYPE
  uint16_t dataReady = 0;

  error = scd30.getDataReady(dataReady);

  if (error) {
    errorToString(error, errorMessage, 256);
    Serial.print("Error: ");
    Serial.println(errorMessage);
    return;
  }

  if (dataReady) {

    float co2 = 0.0;
    float temp = 0.0;
    float hum = 0.0;

    error = scd30.readMeasurementData(co2, temp, hum);

    if (error) {
      errorToString(error, errorMessage, 256);
      Serial.print("Read error: ");
      Serial.println(errorMessage);
    } else {

      Serial.println("------ SCD30 ------");

      Serial.print("CO2: ");
      Serial.print(co2);
      Serial.println(" ppm");

      Serial.print("Temp: ");
      Serial.print(temp);
      Serial.println(" °C");

      Serial.print("Humidity: ");
      Serial.print(hum);
      Serial.println(" %");

      Serial.println("-------------------");
    }
  }

  delay(2000);
}
