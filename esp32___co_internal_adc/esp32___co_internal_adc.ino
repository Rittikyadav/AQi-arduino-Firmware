#define CO_SENSOR_PIN 34

float baselineVoltage = 0.168;
float sensitivity = 300;   // corrected scaling

int readADCStable() {
  long sum = 0;

  for(int i=0;i<20;i++){
    sum += analogRead(CO_SENSOR_PIN);
    delay(2);
  }

  return sum/20;
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("CO Sensor Running...");
}

void loop() {

  int raw = readADCStable();
  float voltage = raw * (3.3 / 4095.0);

  float co_ppm = (voltage - baselineVoltage) * sensitivity;

  if(co_ppm < 0) co_ppm = 0;

  Serial.print("ADC: ");
  Serial.print(raw);

  Serial.print(" | Voltage: ");
  Serial.print(voltage,3);

  Serial.print(" V | CO: ");
  Serial.print(co_ppm);
  Serial.println(" ppm");

  delay(2000);
}
