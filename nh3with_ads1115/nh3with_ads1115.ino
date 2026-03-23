#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// ========== I2C ==========
#define SDA_PIN 21
#define SCL_PIN 22

// ========== ADS1115 ======
#define ADS_SAMPLES 20
#define ADS_LSB 0.000125   // GAIN_ONE → 125 µV / bit

// ========== NH3 SENSOR ===
const float VREF = 5.0;      // NH3 sensor supply
const float RL_NH3 = 0.47;   // MΩ (470kΩ)
const float R0_NH3 = 0.333;  // MΩ (calibrated clean air)

// ---- NH3 curve (TEMP / placeholder) ----
const float A_NH3 = 1.0;
const float B_NH3 = -1.0;

// Safety limits
#define MIN_VALID_V 0.10
#define MAX_VALID_V 4.00
// ========================

// ---- Stable voltage read ----
float readVoltage() {
  long sum = 0;
  for (int i = 0; i < ADS_SAMPLES; i++) {
    sum += ads.readADC_SingleEnded(0); // A0
    delay(2);
  }
  return (sum / (float)ADS_SAMPLES) * ADS_LSB;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  while (Serial.available()) Serial.read();

  Serial.println("\n--- NH3 SENSOR (ADS1115) Rs + PPM ---");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ads.begin()) {
    Serial.println("❌ ADS1115 not detected");
    while (1);
  }

  ads.setGain(GAIN_ONE);   // ±4.096V
  Serial.println("ADS1115 ready (GAIN_ONE)");
  Serial.println("----------------------------------");
}

void loop() {
  float voltage = readVoltage();

  if (voltage < MIN_VALID_V || voltage > MAX_VALID_V) {
    Serial.print("⚠️ Voltage out of range: ");
    Serial.println(voltage, 3);
    delay(2000);
    return;
  }

  // ---- Rs calculation ----
  float Rs = ((VREF - voltage) * (RL_NH3 * 1e6)) / voltage;
  float Rs_M = Rs / 1e6;

  // ---- PPM calculation ----
  float ratio = Rs_M / R0_NH3;
  float ppm = A_NH3 * pow(ratio, B_NH3);
  if (ppm < 0 || isnan(ppm)) ppm = 0;

  // ---- Output ----
  Serial.print("V: ");
  Serial.print(voltage, 5);
  Serial.print(" V | Rs: ");
  Serial.print(Rs_M, 3);
  Serial.print(" MΩ | NH3: ");
  Serial.print(ppm, 3);
  Serial.println(" ppm");

  delay(2000);
}
