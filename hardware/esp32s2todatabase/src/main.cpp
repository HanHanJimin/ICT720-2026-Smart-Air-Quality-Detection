#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_HTS221.h>
#include <Adafruit_Sensor.h>

// WiFi
const char* WIFI_SSID = "true_home2G_672";
const char* WIFI_PASSWORD = "bcfbf672";
const char* FIREBASE_HOST = "besties-591ee-default-rtdb.asia-southeast1.firebasedatabase.app";

// NTP settings
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 7 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ===== Honeywell HPMA115S0 UART pins =====
// ESP32-S2 side
#define HPMA_RX_PIN 18   // ESP32 receives from HPMA TX
#define HPMA_TX_PIN 17   // ESP32 transmits to HPMA RX

// ===== Cucumber onboard I2C pins =====
#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 40

HardwareSerial hpmaSerial(1);
Adafruit_HTS221 hts;

unsigned long lastSend = 0;

// latest PM values from HPMA115S0
uint16_t pm2_5 = 0;
uint16_t pm10  = 0;

// latest onboard sensor values
float boardTemp = NAN;
float boardHumidity = NAN;
bool htsFound = false;

// ---------- WiFi / Time ----------
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void initTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  Serial.print("Waiting for NTP time");
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synced");
}

// ---------- I2C / HTS221 ----------
void scanI2C() {
  Serial.println("Scanning I2C...");
  byte count = 0;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      count++;
    }
  }

  if (count == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.println("I2C scan done");
  }
}

void initHTS221() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(100);

  scanI2C();

  if (!hts.begin_I2C()) {
    Serial.println("HTS221 not found");
    htsFound = false;
    return;
  }

  htsFound = true;
  Serial.println("HTS221 initialized");
}

void updateHTSValues() {
  if (!htsFound) return;

  sensors_event_t humidityEvent;
  sensors_event_t tempEvent;

  if (hts.getEvent(&humidityEvent, &tempEvent)) {
    boardHumidity = humidityEvent.relative_humidity;
    boardTemp = tempEvent.temperature;

    Serial.print("Board Temp: ");
    Serial.print(boardTemp, 2);
    Serial.print(" C, Humidity: ");
    Serial.print(boardHumidity, 2);
    Serial.println(" %");
  } else {
    Serial.println("Failed to read HTS221");
  }
}

// ---------- Firebase ----------
bool firebasePost(String path, String jsonPayload) {
  WiFiClientSecure client;
  client.setInsecure(); // testing only

  HTTPClient https;
  String url = "https://" + String(FIREBASE_HOST) + path + ".json";

  if (!https.begin(client, url)) {
    Serial.println("HTTPS begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  int httpCode = https.POST(jsonPayload);
  String response = https.getString();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);

  https.end();
  return (httpCode >= 200 && httpCode < 300);
}

// ---------- Honeywell HPMA115S0 ----------
uint8_t calcChecksum(uint8_t head, uint8_t len, uint8_t cmd, uint8_t data = 0) {
  return (uint8_t)((65536 - (head + len + cmd + data)) % 256);
}

void hpmaFlushInput() {
  while (hpmaSerial.available()) {
    hpmaSerial.read();
  }
}

bool hpmaWaitForAck(unsigned long timeoutMs = 1000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (hpmaSerial.available() >= 2) {
      uint8_t b1 = hpmaSerial.read();
      uint8_t b2 = hpmaSerial.read();

      if (b1 == 0xA5 && b2 == 0xA5) {
        return true;
      }
      if (b1 == 0x96 && b2 == 0x96) {
        Serial.println("HPMA negative ACK");
        return false;
      }
    }
  }
  Serial.println("HPMA ACK timeout");
  return false;
}

bool hpmaStartMeasurement() {
  hpmaFlushInput();

  // Command from Honeywell protocol:
  // 68 01 01 96
  uint8_t cmd[4] = {0x68, 0x01, 0x01, 0x96};
  hpmaSerial.write(cmd, 4);
  hpmaSerial.flush();

  bool ok = hpmaWaitForAck(2000);
  if (ok) {
    Serial.println("HPMA measurement started");
  }
  return ok;
}

bool hpmaStopMeasurement() {
  hpmaFlushInput();

  // Command:
  // 68 01 02 95
  uint8_t cmd[4] = {0x68, 0x01, 0x02, 0x95};
  hpmaSerial.write(cmd, 4);
  hpmaSerial.flush();

  bool ok = hpmaWaitForAck(2000);
  if (ok) {
    Serial.println("HPMA measurement stopped");
  }
  return ok;
}

bool hpmaReadMeasurement(uint16_t &outPM25, uint16_t &outPM10) {
  hpmaFlushInput();

  // Command from Honeywell protocol:
  // 68 01 04 93
  uint8_t cmd[4] = {0x68, 0x01, 0x04, 0x93};
  hpmaSerial.write(cmd, 4);
  hpmaSerial.flush();

  // Response:
  // 40 05 04 DF1 DF2 DF3 DF4 CS
  // PM2.5 = DF1*256 + DF2
  // PM10  = DF3*256 + DF4
  uint8_t resp[8];
  unsigned long start = millis();
  int idx = 0;

  while ((millis() - start < 2000) && idx < 8) {
    if (hpmaSerial.available()) {
      resp[idx++] = hpmaSerial.read();
    }
  }

  if (idx != 8) {
    Serial.println("HPMA read timeout");
    return false;
  }

  if (resp[0] == 0x96 && resp[1] == 0x96) {
    Serial.println("HPMA read negative ACK");
    return false;
  }

  if (!(resp[0] == 0x40 && resp[1] == 0x05 && resp[2] == 0x04)) {
    Serial.println("HPMA invalid header");
    return false;
  }

  uint8_t cs = (uint8_t)((65536 - (resp[0] + resp[1] + resp[2] + resp[3] + resp[4] + resp[5] + resp[6])) % 256);
  if (cs != resp[7]) {
    Serial.println("HPMA checksum failed");
    return false;
  }

  outPM25 = ((uint16_t)resp[3] << 8) | resp[4];
  outPM10 = ((uint16_t)resp[5] << 8) | resp[6];

  return true;
}

void updateHPMAValues() {
  uint16_t newPM25, newPM10;

  if (hpmaReadMeasurement(newPM25, newPM10)) {
    pm2_5 = newPM25;
    pm10 = newPM10;

    Serial.print("HPMA PM2.5: ");
    Serial.print(pm2_5);
    Serial.print(" ug/m3, PM10: ");
    Serial.print(pm10);
    Serial.println(" ug/m3");
  } else {
    Serial.println("Failed to read HPMA115S0");
  }
}

// ---------- JSON ----------
String makeJsonPayload() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char thaiTime[30];
  strftime(thaiTime, sizeof(thaiTime), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  String json = "{";
  json += "\"time\":\"" + String(thaiTime) + "\",";
  json += "\"pm2_5\":" + String(pm2_5) + ",";
  json += "\"pm10\":" + String(pm10) + ",";

  if (isnan(boardTemp)) {
    json += "\"temp\":null,";
  } else {
    json += "\"temp\":" + String(boardTemp, 2) + ",";
  }

  if (isnan(boardHumidity)) {
    json += "\"humidity\":null";
  } else {
    json += "\"humidity\":" + String(boardHumidity, 2);
  }

  json += "}";
  return json;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // HPMA115S0 UART: 9600 8N1
  hpmaSerial.begin(9600, SERIAL_8N1, HPMA_RX_PIN, HPMA_TX_PIN);

  initHTS221();
  connectWiFi();
  initTime();

  // give sensor time to boot
  delay(10000);

  hpmaStartMeasurement();

  // Honeywell sensors need some warm-up / stabilization time
  delay(5000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    initTime();
  }

  updateHTSValues();
  updateHPMAValues();

  if (millis() - lastSend > 120000) {
    lastSend = millis();

    String payload = makeJsonPayload();

    Serial.println("JSON payload:");
    Serial.println(payload);

    firebasePost("/room2/logs", payload);
  }

  delay(1000);
}