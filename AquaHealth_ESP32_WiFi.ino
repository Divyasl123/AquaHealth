
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================================
// WIFI SETTINGS
// =====================================

const char* WIFI_SSID = "POCO M6 Pro 5G";
const char* WIFI_PASSWORD = "1223334444";

// =====================================
// SENSOR PINS
// =====================================

#define TDS_PIN 34
#define TEMP_PIN 4
#define BUZZER_PIN 23

const bool BUZZER_ACTIVE_LOW = false;

// Demonstration alert threshold only
const float DANGER_TDS = 600.0;

// =====================================
// SENSOR OBJECTS
// =====================================

OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

WebServer server(80);

// =====================================
// VARIABLES
// =====================================

float temperatureC = 25.0;
float tdsValue = 0.0;

bool realTemperatureFound = false;
bool dangerous = false;

int analogValue = 0;

unsigned long lastSensorRead = 0;
unsigned long lastTempUpdate = 0;

float simulatedTemp = 25.0;

// =====================================
// BUZZER
// =====================================

void buzzerOn() {
  digitalWrite(BUZZER_PIN,
    BUZZER_ACTIVE_LOW ? LOW : HIGH);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN,
    BUZZER_ACTIVE_LOW ? HIGH : LOW);
}

// =====================================
// SIMULATED TEMPERATURE
// Changes every 60 seconds
// Only used if DS18B20 is not detected
// =====================================

void updateSimulatedTemperature() {

  if (millis() - lastTempUpdate >= 60000) {

    lastTempUpdate = millis();

    simulatedTemp += 0.2;

    if (simulatedTemp > 25.8) {
      simulatedTemp = 25.0;
    }
  }
}

// =====================================
// READ TEMPERATURE
// =====================================

void readTemperature() {

  tempSensor.requestTemperatures();

  float temp = tempSensor.getTempCByIndex(0);

  if (temp != DEVICE_DISCONNECTED_C &&
      temp > -55 && temp < 125) {

    temperatureC = temp;
    realTemperatureFound = true;

  } else {

    realTemperatureFound = false;

    updateSimulatedTemperature();

    temperatureC = simulatedTemp;
  }
}

// =====================================
// READ TDS
// =====================================

void readTDS() {

  // Average 10 ADC samples
  long total = 0;

  for (int i = 0; i < 10; i++) {
    total += analogRead(TDS_PIN);
    delay(5);
  }

  analogValue = total / 10;

  float voltage =
    analogReadMilliVolts(TDS_PIN) / 1000.0;

  float compensationCoefficient =
    1.0 + 0.02 * (temperatureC - 25.0);

  float compensatedVoltage =
    voltage / compensationCoefficient;

  // Approximate TDS calculation
  float v = compensatedVoltage;

  float calculatedTDS =
    (133.42 * v * v * v
     - 255.86 * v * v
     + 857.39 * v) * 0.5;

  if (calculatedTDS < 0) {
    calculatedTDS = 0;
  }

  tdsValue = calculatedTDS;
}

// =====================================
// READ ALL SENSORS
// =====================================

void readSensors() {

  readTemperature();

  readTDS();

  dangerous = (tdsValue > DANGER_TDS);

  if (dangerous) {
    buzzerOn();
  } else {
    buzzerOff();
  }
}

// =====================================
// ROOT WEB PAGE
// =====================================

void handleRoot() {

  server.sendHeader(
    "Access-Control-Allow-Origin", "*");

  server.send(
    200,
    "text/plain",
    "AquaHealth ESP32 is online!"
  );
}

// =====================================
// SEND JSON TO AQUAHEALTH DASHBOARD
// =====================================

void handleReadings() {

  String json = "{";

  json += "\"tdsRaw\":";
  json += String(analogValue);
  json += ",";

  json += "\"tdsPpmEstimate\":";
  json += String(tdsValue, 1);
  json += ",";

  // True means a displayable temperature exists.
  json += "\"temperatureValid\":true,";

  json += "\"temperatureC\":";
  json += String(temperatureC, 1);
  json += ",";

  json += "\"temperatureSimulated\":";
  json += (realTemperatureFound ? "false" : "true");
  json += ",";

  json += "\"buzzerOn\":";
  json += (dangerous ? "true" : "false");

  json += "}";

  server.sendHeader(
    "Access-Control-Allow-Origin", "*");

  server.sendHeader(
    "Cache-Control", "no-store");

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  analogReadResolution(12);

  analogSetPinAttenuation(
    TDS_PIN, ADC_11db);

  tempSensor.begin();

  Serial.println();
  Serial.println("===== AQUAHEALTH SYSTEM =====");

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("Connecting to Wi-Fi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED &&
         attempts < 40) {

    delay(500);
    Serial.print(".");

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("Wi-Fi connected!");

    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handleRoot);

    server.on(
      "/api/readings",
      HTTP_GET,
      handleReadings
    );

    server.begin();

    Serial.println(
      "AquaHealth web server started!");

  } else {

    Serial.println();
    Serial.println("Wi-Fi connection failed!");
  }

  readSensors();

  lastSensorRead = millis();
  lastTempUpdate = millis();
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  if (millis() - lastSensorRead >= 2000) {

    lastSensorRead = millis();

    readSensors();

    Serial.println("-----------------------------");

    Serial.print("Temperature: ");
    Serial.print(temperatureC, 1);
    Serial.print(" C");

    if (realTemperatureFound) {
      Serial.println(" (DS18B20)");
    } else {
      Serial.println(" (SIMULATED)");
    }

    Serial.print("TDS Analog: ");
    Serial.println(analogValue);

    Serial.print("Estimated TDS: ");
    Serial.print(tdsValue, 1);
    Serial.println(" ppm");

    Serial.print("BUZZER: ");
    Serial.println(dangerous ? "ON" : "OFF");

    Serial.println("-----------------------------");
  }
}
