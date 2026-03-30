#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// ---------- WiFi & Firebase ----------
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
const char* FIREBASE_HOST = "FIREBASE_HOST";

// ---------- NTP Settings ----------
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 7 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ---------- Pin Definitions (ESP8266 / NodeMCU) ----------
// PMS7003 Air Quality Sensor
#define PMS_RX_PIN 14 // GPIO14 (D5) - Connect to PMS TX
#define PMS_TX_PIN 12 // GPIO12 (D6) - Connect to PMS RX

// DHT22 Temperature & Humidity Sensor
#define DHTPIN 13     // GPIO13 (D7) - Connect to DHT22 Data pin
#define DHTTYPE DHT22 // DHT 22 (AM2302)

// I2C Pins (Used for 1602 LCD)
#define I2C_SDA_PIN 4 // GPIO4 (D2)
#define I2C_SCL_PIN 5 // GPIO5 (D1)

// ---------- Object Initialization ----------
SoftwareSerial pmsSerial(PMS_RX_PIN, PMS_TX_PIN);
DHT dht(DHTPIN, DHTTYPE);
// Set the LCD address to 0x27 for a 16 chars and 2 line display. 
// (If screen is blank later, try address 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// ---------- Global Variables ----------
unsigned long lastSend = 0;
unsigned long lastDhtRead = 0;
unsigned long lastLcdUpdate = 0;

uint16_t pm2_5 = 0;
uint16_t pm10  = 0;

float boardTemp = NAN;
float boardHumidity = NAN;

// ---------- WiFi & Time ----------
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi.");
  
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(dotCount % 16, 1);
    lcd.print(".");
    dotCount++;
  }
  Serial.println("\nWiFi connected");
  lcd.clear();
  lcd.print("WiFi Connected!");
}

void initTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  Serial.print("Waiting for NTP time");
  lcd.setCursor(0, 1);
  lcd.print("Syncing Time... ");
  
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) { 
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synced");
  lcd.clear();
}

// ---------- Sensors ----------
void updateDHTValues() {
  // DHT22 is a slow sensor, only read every 2 seconds
  if (millis() - lastDhtRead > 2000) {
    lastDhtRead = millis();
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      boardHumidity = h;
      boardTemp = t;
      Serial.print("DHT Temp: ");
      Serial.print(boardTemp, 1);
      Serial.print(" C, Humidity: ");
      Serial.print(boardHumidity, 1);
      Serial.println(" %");
    }
  }
}

bool pmsReadMeasurement(uint16_t &outPM25, uint16_t &outPM10) {
  unsigned long start = millis();
  
  while (millis() - start < 2000) {
    if (pmsSerial.available() >= 32) {
      if (pmsSerial.read() == 0x42) {
        if (pmsSerial.peek() == 0x4D) {
          pmsSerial.read(); 
          
          uint8_t buf[30];
          pmsSerial.readBytes(buf, 30);

          uint16_t calcChecksum = 0x42 + 0x4D;
          for (int i = 0; i < 28; i++) {
            calcChecksum += buf[i];
          }

          uint16_t expectedChecksum = (buf[28] << 8) | buf[29];

          if (calcChecksum == expectedChecksum) {
            outPM25 = (buf[10] << 8) | buf[11];
            outPM10 = (buf[12] << 8) | buf[13];
            
            while (pmsSerial.available()) pmsSerial.read();
            return true;
          }
        }
      }
    }
    delay(10);
  }
  return false;
}

void updatePMSValues() {
  uint16_t newPM25, newPM10;

  if (pmsReadMeasurement(newPM25, newPM10)) {
    pm2_5 = newPM25;
    pm10 = newPM10;
    Serial.printf("PMS PM2.5: %d, PM10: %d\n", pm2_5, pm10);
  }
}

// ---------- LCD Display ----------
void updateLCD() {
  // Refresh LCD every 2 seconds to prevent flickering
  if (millis() - lastLcdUpdate > 2000) {
    lastLcdUpdate = millis();
    
    char line1[17];
    char line2[17];
    
    // Format: "PM2.5: 15 T:24.5C"
    snprintf(line1, sizeof(line1), "PM2.5:%-3d T:%.1fC", pm2_5, isnan(boardTemp) ? 0.0 : boardTemp);
    // Format: "PM10 : 20 H:60.2%"
    snprintf(line2, sizeof(line2), "PM10 :%-3d H:%.1f%%", pm10, isnan(boardHumidity) ? 0.0 : boardHumidity);
    
    lcd.setCursor(0, 0);
    lcd.print(line1);
    
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

// ---------- Firebase HTTP POST ----------
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

  Serial.print("HTTP POST: ");
  Serial.println(httpCode);

  https.end();
  return (httpCode >= 200 && httpCode < 300);
}

// ---------- JSON Payload ----------
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

// ---------- Main Logic ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize I2C and LCD
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting..");

  // Initialize Sensors
  dht.begin();
  pmsSerial.begin(9600);

  // Connect to Network & Time
  connectWiFi();
  initTime();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    initTime(); 
  }

  updateDHTValues();
  updatePMSValues();
  updateLCD();

  // Send to Firebase every 2 minutes (120000 ms)
  if (millis() - lastSend > 120000) {
    lastSend = millis();

    String payload = makeJsonPayload();
    Serial.println("Sending payload: " + payload);
    firebasePost("/room1/logs", payload);
  }

  delay(100); 
}