/****************************************************
 *  BME688 Environmental Sensor – Diagnostic Test
 *  Platform : ESP32
 *  Interface: I2C
 *  Purpose  : Validate temperature, humidity,
 *             pressure and gas resistance
 ****************************************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

/* =======================
   I2C CONFIGURATION
   ======================= */
#define I2C_SDA 21
#define I2C_SCL 22
#define BME688_ADDR 0x76   // Change to 0x77 if required

/* =======================
   OBJECT DECLARATION
   ======================= */
Adafruit_BME680 bme;

/* =======================
   FUNCTION PROTOTYPES
   ======================= */
void initI2C();
void initBME688();
void readAndPrintBME688();

/* =======================
   SETUP
   ======================= */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println(" BME688 ENVIRONMENTAL SENSOR TEST");
  Serial.println("======================================");

  initI2C();
  initBME688();
}

/* =======================
   LOOP
   ======================= */
void loop() {
  readAndPrintBME688();
  delay(2000);
}

/* =======================
   I2C INITIALIZATION
   ======================= */
void initI2C() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);  // Standard I2C speed
  Serial.println("✅ I2C initialized");
}

/* =======================
   BME688 INITIALIZATION
   ======================= */
void initBME688() {
  if (!bme.begin(BME688_ADDR)) {
    Serial.println("❌ ERROR: BME688 not detected");
    Serial.println("➡️ Check wiring or I2C address");
    while (true);  // Halt execution
  }

  /* Oversampling configuration */
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  /* Gas sensor heater */
  bme.setGasHeater(320, 150);  // 320°C for 150 ms

  Serial.println("✅ BME688 initialized successfully");
}

/* =======================
   READ & DISPLAY DATA
   ======================= */
void readAndPrintBME688() {

  if (!bme.performReading()) {
    Serial.println("❌ Sensor reading failed");
    return;
  }

  Serial.println("\n--------------------------------------");
  Serial.print("🌡 Temperature        : ");
  Serial.print(bme.temperature, 2);
  Serial.println(" °C");

  Serial.print("💧 Humidity           : ");
  Serial.print(bme.humidity, 2);
  Serial.println(" %");

  Serial.print("⛰ Pressure           : ");
  Serial.print(bme.pressure / 100.0, 2);
  Serial.println(" hPa");

  Serial.print("🧪 Gas Resistance     : ");
  Serial.print(bme.gas_resistance / 1000.0, 2);
  Serial.println(" kΩ");
  Serial.println("--------------------------------------");
}
