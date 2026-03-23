#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// ===== USER CONFIG =====
float baselineVoltage = 0.050000;
float sensitivity = 300;

// ===== FILTER CONFIG =====
#define NUM_SAMPLES 5

float filteredCO = 0;

// tighter hysteresis for better baseline tracking
const float hysteresis = 0.003;     
const float baselineAlpha = 0.001;  

// ===== FUNCTION: Median Filter =====
float getMedian(float *arr, int size) {
  float temp[size];
  memcpy(temp, arr, size * sizeof(float));

  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (temp[i] > temp[j]) {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }
  return temp[size / 2];
}

// ===== FUNCTION: Stable Voltage Read =====
float readStableVoltage() {

  float samples[NUM_SAMPLES];

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(0);
    samples[i] = ads.computeVolts(raw);
    delay(5);
  }

  return getMedian(samples, NUM_SAMPLES);
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);
  Wire.begin(21,22);

  if (!ads.begin()) {
    Serial.println("ADS1115 not detected!");
    while (1);
  }

  ads.setGain(GAIN_EIGHT);

  Serial.println("===== CO Sensor AQI Module Started =====");
}

// ===== LOOP =====
void loop() {

  float voltage = readStableVoltage();

  float delta = voltage - baselineVoltage;

  // ===== Rolling Baseline (with hysteresis) =====
  if (abs(delta) < hysteresis) {
    baselineVoltage =
      (baselineVoltage * (1.0 - baselineAlpha)) +
      (voltage * baselineAlpha);
  }

  // ===== CO ppm calculation =====
  float co_ppm = delta * sensitivity;

  if (co_ppm < 0) co_ppm = 0;

  // ===== Exponential smoothing =====
  filteredCO = (filteredCO * 0.8) + (co_ppm * 0.2);

  // ===== Output =====
  Serial.print("Voltage: ");
  Serial.print(voltage,6);

  Serial.print(" | Baseline: ");
  Serial.print(baselineVoltage,6);

  Serial.print(" | CO Raw: ");
  Serial.print(co_ppm,2);

  Serial.print(" ppm | CO Filtered: ");
  Serial.print(filteredCO,2);

  Serial.println(" ppm");

  delay(5000);
}
