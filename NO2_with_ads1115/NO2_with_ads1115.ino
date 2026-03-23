#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

#define SDA_PIN 21
#define SCL_PIN 22

#define ADS_SAMPLES 20
#define ADS_LSB 0.0000625   // GAIN_TWO

const float VREF = 3.3;
const float RL_No2 = 0.47;   // MΩ
const float R0_No2 = 4.11;   // MΩ (calibrated)

const float A = 0.97514;
const float B = -1.32562;

#define MIN_VALID_V 0.1
#define MAX_VALID_V 1.2

float readVoltage() {
  long sum = 0;
  for (int i = 0; i < ADS_SAMPLES; i++) {
    sum += ads.readADC_SingleEnded(0);
    delay(2);
  }
  return (sum / (float)ADS_SAMPLES) * ADS_LSB;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  while (Serial.available()) Serial.read();

  Serial.println("\n--- NO2 V2 (ADS1115) FINAL TEST ---");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ads.begin()) {
    Serial.println("ADS1115 not found");
    while (1);
  }

  ads.setGain(GAIN_TWO);
}

void loop() {
  float voltage = readVoltage();

  if (voltage < MIN_VALID_V || voltage > MAX_VALID_V) {
    Serial.println("Invalid voltage");
    delay(2000);
    return;
  }

  float Rs = ((VREF - voltage) * (RL_No2 * 1e6)) / voltage;
  float Rs_M = Rs / 1e6;

  float ratio = Rs_M / R0_No2;
  float ppm = A * pow(ratio, B);
  if (ppm < 0 || isnan(ppm)) ppm = 0;

  Serial.print("V: ");
  Serial.print(voltage, 5);
  Serial.print(" V | Rs: ");
  Serial.print(Rs_M, 3);
  Serial.print(" MΩ | NO2: ");
  Serial.print(ppm, 3);
  Serial.println(" ppm");

  delay(2000);
}
