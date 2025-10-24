#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <LittleFS.h> 

#define SEALEVELPRESSURE_HPA (1013.25)

// ====== SETTINGS ======
const char* LOG_PATH = "/bme680.csv";
const uint32_t SAMPLE_INTERVAL_MS = 10000;   // 10 seconds
// If your BME680 is at 0x76, use: Adafruit_BME680 bme(BME680_I2C_ADDR_SECONDARY);
Adafruit_BME680 bme; // default I2C addr 0x77

uint32_t lastSample = 0;

// Create file w/ header if it doesn't exist
void writeCsvHeaderIfNew() {
  if (!LittleFS.exists(LOG_PATH)) {
    File f = LittleFS.open(LOG_PATH, "w");   // create/overwrite
    if (f) {
      f.println(F("millis,temp_F,humidity_pct,pressure_hPa,gas_ohms,altitude_m"));
      f.close();
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println(F("\nBME680 -> LittleFS Flash Logger (°F, 10s interval)"));

  // ---- Mount LittleFS ----
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed. Formatting..."));
    LittleFS.format();
    if (!LittleFS.begin()) {
      Serial.println(F("ERROR: LittleFS mount failed after format."));
      while (1) { delay(10); }
    }
  }
  Serial.println(F("LittleFS mounted."));
  writeCsvHeaderIfNew();

  // ---- BME680 init ----
  if (!bme.begin()) {
    Serial.println(F("ERROR: BME680 not found on I2C. Check wiring/address (0x77/0x76)."));
    while (1) { delay(10); }
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320°C for 150ms

  Serial.println(F("Setup complete. Type 'd' in Serial Monitor to dump CSV."));
}

void logOneSample() {
  if (!bme.performReading()) {
    Serial.println(F("WARN: BME680 reading failed."));
    return;
  }

  float tempC        = bme.temperature;
  float tempF        = (tempC * 9.0 / 5.0) + 32.0;   // Convert to °F
  float humidity     = bme.humidity;
  float pressure_hPa = bme.pressure / 100.0;
  float gas_ohms     = bme.gas_resistance;          // ohms
  float altitude_m   = bme.readAltitude(SEALEVELPRESSURE_HPA);

  // Print to Serial
  Serial.print(F("t(ms)=")); Serial.print(millis());
  Serial.print(F(", T="));    Serial.print(tempF, 2); Serial.print(F("F"));
  Serial.print(F(", RH="));   Serial.print(humidity, 2); Serial.print(F("%"));
  Serial.print(F(", P="));    Serial.print(pressure_hPa, 2); Serial.print(F("hPa"));
  Serial.print(F(", Gas="));  Serial.print(gas_ohms, 0); Serial.print(F("Ω"));
  Serial.print(F(", Alt="));  Serial.print(altitude_m, 2); Serial.println(F("m"));

  // Append to CSV
  File f = LittleFS.open(LOG_PATH, "a"); // append mode
  if (f) {
    f.print(millis());           f.print(',');
    f.print(tempF, 2);           f.print(',');   // log Fahrenheit
    f.print(humidity, 2);        f.print(',');
    f.print(pressure_hPa, 2);    f.print(',');
    f.print(gas_ohms, 0);        f.print(',');
    f.println(altitude_m, 2);
    f.close();
  } else {
    Serial.println(F("ERROR: Could not open log file for append."));
  }
}

void dumpFileToSerial() {
  if (!LittleFS.exists(LOG_PATH)) {
    Serial.println(F("No log file found."));
    return;
  }
  File f = LittleFS.open(LOG_PATH, "r"); // read mode
  if (!f) {
    Serial.println(F("ERROR: Could not open log file for read."));
    return;
  }
  Serial.println(F("\n----- BEGIN /bme680.csv -----"));
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println(F("----- END /bme680.csv -----\n"));
  f.close();
}

void loop() {
  // Command: 'd' to dump CSV to Serial
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'd' || c == 'D') {
      dumpFileToSerial();
    }
  }

  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    logOneSample();
  }

  delay(25); // tiny idle
}
