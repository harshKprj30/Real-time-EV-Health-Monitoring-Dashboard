/*
 * Real-time EV Health Monitoring System
 * NodeMCU ESP8266 Firmware
 * Author: harshKprj30 | Version: 2.3.1
 *
 * Libraries required (install via Arduino Library Manager):
 *   - FirebaseESP8266  by Mobizt
 *   - INA219           by flav1972
 *   - DallasTemperature + OneWire
 *   - ArduinoOTA       (built-in with ESP8266 core)
 *
 * Board: NodeMCU 1.0 (ESP-12E Module)
 * Board URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 */

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <INA219.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>

// ── Credentials (fill before flashing) ───────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define FIREBASE_HOST   "your-project-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH   "your-database-secret-or-token"

// ── Pin definitions ───────────────────────────────────
#define ONE_WIRE_BUS    D4    // DS18B20 data line
#define STATUS_LED      LED_BUILTIN

// ── Thresholds ────────────────────────────────────────
#define PUSH_INTERVAL   2000  // ms between Firebase pushes
#define BATT_FULL_V     420.0 // Voltage at 100% SoC (adjust for your pack)
#define BATT_EMPTY_V    320.0 // Voltage at   0% SoC
#define ALERT_TEMP      80.0  // Motor temp alert threshold (°C)
#define ALERT_SOC       15.0  // Low SoC alert threshold (%)

// ── Objects ───────────────────────────────────────────
FirebaseData      fbData;
FirebaseConfig    fbConfig;
FirebaseAuth      fbAuth;
INA219            ina219;
OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

unsigned long lastPush    = 0;
unsigned long startMillis = 0;

// ── SoC estimation via OCV lookup ────────────────────
float estimateSoC(float voltage) {
  if (voltage >= BATT_FULL_V)  return 100.0f;
  if (voltage <= BATT_EMPTY_V) return 0.0f;
  return ((voltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V)) * 100.0f;
}

String getUptime() {
  unsigned long sec = (millis() - startMillis) / 1000;
  return String(sec / 3600) + "h " + String((sec % 3600) / 60) + "m";
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH); // Active-LOW on NodeMCU

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500); Serial.print("."); retries++;
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Firebase
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  // Sensors
  ina219.begin();
  tempSensors.begin();
  Serial.println("Sensors: " + String(tempSensors.getDeviceCount()) + " DS18B20 found");

  // OTA (update firmware over WiFi)
  ArduinoOTA.setHostname("EV-Monitor");
  ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error[%u]\n", e); });
  ArduinoOTA.begin();

  startMillis = millis();
  digitalWrite(STATUS_LED, LOW);  // LED ON = ready
  Serial.println("EV Monitor ready.");
}

void loop() {
  ArduinoOTA.handle();

  if (millis() - lastPush >= PUSH_INTERVAL) {
    lastPush = millis();

    // INA219 readings (scaled to full pack)
    float busVoltage = ina219.getBusVoltage_V() * 100.0f; // scale factor for your pack
    float current    = ina219.getCurrent_mA() / 1000.0f;  // A (negative = charging)
    float powerKW    = abs(busVoltage * current) / 1000.0f;

    // DS18B20 readings
    tempSensors.requestTemperatures();
    float battTemp  = tempSensors.getTempCByIndex(0);
    float motorTemp = tempSensors.getTempCByIndex(1);

    // Derived metrics
    float soc      = estimateSoC(busVoltage);
    float estRange = soc * 3.1f;   // tweak factor for your EV (~310 km at 100%)
    bool  charging = (current < 0);

    // Push all readings atomically
    Firebase.setFloat(fbData,  "/ev/voltage",   busVoltage);
    Firebase.setFloat(fbData,  "/ev/current",   abs(current));
    Firebase.setFloat(fbData,  "/ev/power",     powerKW);
    Firebase.setFloat(fbData,  "/ev/soc",       soc);
    Firebase.setFloat(fbData,  "/ev/range",     estRange);
    Firebase.setFloat(fbData,  "/ev/battTemp",  battTemp);
    Firebase.setFloat(fbData,  "/ev/motorTemp", motorTemp);
    Firebase.setBool(fbData,   "/ev/charging",  charging);
    Firebase.setInt(fbData,    "/ev/rssi",      WiFi.RSSI());
    Firebase.setString(fbData, "/ev/uptime",    getUptime());
    Firebase.setInt(fbData,    "/ev/ts",        (int)(millis() / 1000));

    // Alerts
    if (motorTemp > ALERT_TEMP)
      Firebase.setString(fbData, "/ev/alerts/motorTemp",
        "HIGH: " + String(motorTemp, 1) + " C at " + getUptime());
    if (soc < ALERT_SOC)
      Firebase.setString(fbData, "/ev/alerts/soc",
        "LOW: " + String(soc, 1) + "% at " + getUptime());

    Serial.printf("[EV] V=%.1fV  I=%.2fA  P=%.2fkW  SoC=%.1f%%  BT=%.1fC  MT=%.1fC  RSSI=%d\n",
      busVoltage, current, powerKW, soc, battTemp, motorTemp, WiFi.RSSI());

    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); // heartbeat blink
  }
}
