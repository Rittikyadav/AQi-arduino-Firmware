#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// ===== BASELINE =====
float baseline = 0;
bool baseline_set = false;

// ===== FILTER SETTINGS =====
#define WINDOW 10
float ppmReadings[WINDOW];
int ppmIndex = 0;

// ===== FILTER FUNCTION =====
float getFiltered(float newVal) {
  ppmReadings[ppmIndex] = newVal;
  ppmIndex = (ppmIndex + 1) % WINDOW;

  float sum = 0;
  for (int i = 0; i < WINDOW; i++) {
    sum += ppmReadings[i];
  }

  return sum / WINDOW;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!ads.begin()) {
    Serial.println("❌ ADS1115 NOT FOUND");
    while (1);
  }

  // ✅ SAFE FULL RANGE (NO CLIPPING EVER)
  ads.setGain(GAIN_ONE);   // ±4.096V

  Serial.println("✅ VOC SYSTEM READY");
  Serial.println("⏳ Keep sensor in CLEAN AIR for baseline...");
  delay(10000);
}

void loop() {

  static int count = 0;
  static float sum = 0;

  int16_t adc = ads.readADC_SingleEnded(0);
  float voltage = adc * 0.125 / 1000;

  // ===== BASELINE CALC =====
  if (!baseline_set) {
    sum += voltage;
    count++;

    Serial.print("Calibrating ");
    Serial.print(count);
    Serial.print("/50 → ");
    Serial.println(voltage, 5);

    if (count >= 50) {
      baseline = sum / count;
      baseline_set = true;

      Serial.println("\n=======================");
      Serial.print("✅ BASELINE: ");
      Serial.print(baseline, 5);
      Serial.println(" V");
      Serial.println("=======================\n");
    }

    delay(200);
    return;
  }

  // ===== CALCULATIONS =====
  float ratio = voltage / baseline;

  float deviation = voltage - baseline;
  if (deviation < 0) deviation = 0;

  float voc_index = deviation * 1000;

  // 🔥 RAW PPM ESTIMATION
  float raw_ppm = 100 * pow(ratio, 2.5);

  // 🔥 FILTERED PPM
  float voc_ppm = getFiltered(raw_ppm);

  // ===== OUTPUT =====
  Serial.print("Voltage: ");
  Serial.print(voltage, 5);

  Serial.print(" V | Ratio: ");
  Serial.print(ratio, 3);

  Serial.print(" | VOC Index: ");
  Serial.print(voc_index);

  Serial.print(" | VOC ppm (raw): ");
  Serial.print(raw_ppm);

  Serial.print(" | VOC ppm (filtered): ");
  Serial.println(voc_ppm);

  delay(1000);
}
