#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "Audio.h"   // ESP32-audioI2S by schreibfaul1

// =====================================================================
// ⚙️  CẤU HÌNH — SỬA 2 DÒNG NÀY TRƯỚC KHI NẠP
// =====================================================================
const char* WIFI_SSID = "Tom's Beloved";
const char* WIFI_PASS = "nhatanhne";

// =====================================================================
// PIN ĐỊNH NGHĨA — ESP32-S3-BOX
// =====================================================================
#define I2S_BCLK  17
#define I2S_LRC   47
#define I2S_DOUT  15
#define I2C_SDA    8
#define I2C_SCL   18
#define PA_PIN    46    // Power Amplifier (loa)
#define LCD_BL    45    // Backlight màn hình
#define BOOT_PIN   0    // Nút boot đổi phòng

// =====================================================================
// MÀN HÌNH ST7789
// =====================================================================
class LGFX_S3Box : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
public:
    LGFX_S3Box() {
        {
            auto cfg       = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.pin_sclk   = 7;
            cfg.pin_mosi   = 6;
            cfg.pin_dc     = 4;
            cfg.freq_write = 40000000;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg            = _panel_instance.config();
            cfg.pin_cs          = 5;
            cfg.pin_rst         = 48;
            cfg.pin_busy        = -1;
            cfg.panel_width     = 240;
            cfg.panel_height    = 320;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;
            cfg.invert          = false;   
            cfg.rgb_order       = false;
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};
LGFX_S3Box tft;
Audio audio;

// =====================================================================
// MÀU RGB565 
// =====================================================================
#define C_SKIN_MAIN   0xFED6   
#define C_SKIN_SHADOW 0xED4E   
#define C_SKIN_HI     0xFF7A   
#define C_SKIN_SHADE  0xF633   
#define C_BROW        0x69C3   
#define C_IRIS        0x3357   
#define C_PUPIL       0x18C5   
#define C_BLUSH       0xF453   
#define C_BLUSH_IN    0xFCB3   
#define C_NOSE        0xD48F   
#define C_LIP         0xCA0C   
#define C_LIP_DARK    0x88C6   
#define C_LIP_HI      0xFF7A   
#define C_TONGUE      0xF310   

#define C_PANEL_BG    0x0C19   
#define C_PANEL_BD    0x1C3A   
#define C_DIVIDER     0x2856
#define C_LABEL       0x6B6D
#define C_PM25_G      0x07E0   
#define C_PM25_Y      0xFFE0   
#define C_PM25_O      0xFD20   
#define C_PM25_R      0xF800   
#define C_PM10_COL    0xB81F   
#define C_TEMP_COL    0xFD20   
#define C_HUM_COL     0x07FF   

// =====================================================================
// LAYOUT NÉN
// =====================================================================
#define SPLIT_X   140    
#define FACE_CX   215    
#define FACE_CY   105    // Dời tâm mặt xuống một chút để nhường chỗ cho chữ ROOM phía trên

// =====================================================================
// TRẠNG THÁI
// =====================================================================
String g_room = "1";
String g_pm25="N/A", g_pm10="N/A", g_temp="N/A", g_hum="N/A", g_time="--:--";
bool   g_eyesOpen    = true;
bool   g_isTalking   = false;
int    g_mouthFrame  = 0;
bool   g_needFace    = true;
bool   g_needSensor  = true;
unsigned long lastBlinkMs   = 0;
unsigned long blinkInterval = 4000;
unsigned long lastAnimMs    = 0;
bool   lastBtnState  = HIGH;

// =====================================================================
// KHỞI TẠO ES8311 QUA I2C 
// =====================================================================
void initES8311() {
    Wire.begin(I2C_SDA, I2C_SCL);
    const uint8_t addr = 0x18;
    auto wr = [&](uint8_t reg, uint8_t val) {
        Wire.beginTransmission(addr);
        Wire.write(reg); Wire.write(val);
        Wire.endTransmission();
        delay(2);
    };
    Serial.println("⚙️  Cấu hình ES8311...");
    wr(0x01, 0x30);  
    delay(20);
    wr(0x01, 0x00);  
    wr(0x02, 0x00);  
    wr(0x03, 0x10);  
    wr(0x0D, 0x02);  
    wr(0x04, 0x10);  
    wr(0x14, 0x1A);  
    wr(0x12, 0x00);  
    wr(0x0E, 0x02);  
    Serial.println("✅ ES8311 OK!");
}

void bezier2(int x0,int y0,int x1,int y1,int x2,int y2, int r, uint32_t col, int steps=40) {
    for (int i=0; i<=steps; i++) {
        float t=i/(float)steps, mt=1.0f-t;
        int px=(int)(mt*mt*x0 + 2*t*mt*x1 + t*t*x2);
        int py=(int)(mt*mt*y0 + 2*t*mt*y1 + t*t*y2);
        tft.fillCircle(px, py, r, col);
    }
}

// =====================================================================
// VẼ KHUÔN MẶT VÀ TIÊU ĐỀ PHÒNG
// =====================================================================
void drawFace(bool eyesOpen, bool isTalking, int mouthFrame) {
    tft.startWrite();
    tft.fillRect(SPLIT_X+1, 0, 319-SPLIT_X, 191, TFT_BLACK);

    // --- VẼ CHỮ "ROOM 1" / "ROOM 2" LÊN ĐẦU KHUÔN MẶT ---
    tft.setTextSize(1.5);
    tft.setTextColor(TFT_ORANGE); 
    tft.setCursor(FACE_CX - 35, 8); // Căn ra giữa vùng mặt, cách mí trên 8px
    tft.print("ROOM " + g_room);

    // Khuôn mặt
    tft.fillCircle(FACE_CX, FACE_CY+2, 80, C_SKIN_SHADOW);
    tft.fillCircle(FACE_CX, FACE_CY-1, 78, C_SKIN_SHADE);
    tft.fillCircle(FACE_CX, FACE_CY,   76, C_SKIN_MAIN);
    tft.fillCircle(FACE_CX, FACE_CY-8, 68, C_SKIN_HI);

    bezier2(FACE_CX-42, FACE_CY-38, FACE_CX-25, FACE_CY-46, FACE_CX-8,  FACE_CY-40, 3, C_BROW, 16);
    bezier2(FACE_CX+8,  FACE_CY-40, FACE_CX+25, FACE_CY-46, FACE_CX+42, FACE_CY-38, 3, C_BROW, 16);

    int lx = FACE_CX-28, ly = FACE_CY-10;
    if (eyesOpen) {
        tft.fillEllipse(lx, ly+4, 24, 22, C_SKIN_SHADOW);
        tft.fillEllipse(lx, ly,   23, 22, TFT_WHITE);
        tft.fillCircle (lx, ly+2, 13, C_IRIS);
        tft.drawCircle (lx, ly+2, 13, C_PUPIL);
        tft.drawCircle (lx, ly+2, 12, C_PUPIL);
        tft.fillCircle (lx+1, ly+1, 7, C_PUPIL);
        tft.fillCircle (lx+5, ly-5, 4, TFT_WHITE);
        tft.fillCircle (lx-4, ly+6, 2, 0xCE59);
        bezier2(lx-23, ly-5, lx, ly-16, lx+23, ly-5, 2, C_PUPIL, 20);
    } else {
        bezier2(lx-22, ly-4, lx, ly+10, lx+22, ly-4, 2, C_BROW, 20);
        tft.drawLine(lx-14, ly-3, lx-17, ly-9, C_BROW);
        tft.drawLine(lx,    ly-2, lx,    ly-8, C_BROW);
        tft.drawLine(lx+14, ly-3, lx+17, ly-9, C_BROW);
    }

    int rx = FACE_CX+28, ry = FACE_CY-10;
    if (eyesOpen) {
        tft.fillEllipse(rx, ry+4, 24, 22, C_SKIN_SHADOW);
        tft.fillEllipse(rx, ry,   23, 22, TFT_WHITE);
        tft.fillCircle (rx, ry+2, 13, C_IRIS);
        tft.drawCircle (rx, ry+2, 13, C_PUPIL);
        tft.drawCircle (rx, ry+2, 12, C_PUPIL);
        tft.fillCircle (rx+1, ry+1, 7, C_PUPIL);
        tft.fillCircle (rx+5, ry-5, 4, TFT_WHITE);
        tft.fillCircle (rx-4, ry+6, 2, 0xCE59);
        bezier2(rx-23, ry-5, rx, ry-16, rx+23, ry-5, 2, C_PUPIL, 20);
    } else {
        bezier2(rx-22, ry-4, rx, ry+10, rx+22, ry-4, 2, C_BROW, 20);
        tft.drawLine(rx-14, ry-3, rx-17, ry-9, C_BROW);
        tft.drawLine(rx,    ry-2, rx,    ry-8, C_BROW);
        tft.drawLine(rx+14, ry-3, rx+17, ry-9, C_BROW);
    }

    tft.fillEllipse(FACE_CX-52, FACE_CY+14, 20, 10, C_BLUSH);
    tft.fillEllipse(FACE_CX-52, FACE_CY+14, 13,  6, C_BLUSH_IN);
    tft.fillEllipse(FACE_CX+52, FACE_CY+14, 20, 10, C_BLUSH);
    tft.fillEllipse(FACE_CX+52, FACE_CY+14, 13,  6, C_BLUSH_IN);

    tft.fillCircle(FACE_CX-7, FACE_CY+22, 4, C_NOSE);
    tft.fillCircle(FACE_CX+7, FACE_CY+22, 4, C_NOSE);
    tft.fillCircle(FACE_CX,   FACE_CY+18, 3, C_SKIN_SHADOW);

    int mx = FACE_CX, my = FACE_CY+38;
    if (isTalking) {
        static const int mh[] = {10, 18, 24, 18};
        int h = mh[mouthFrame % 4], mw = 48;
        tft.fillRoundRect(mx-mw/2-1, my-1, mw+2, h+2, (h+2)/2, C_LIP_DARK);
        tft.fillRoundRect(mx-mw/2,   my,   mw,   h,   h/2,     C_LIP);
        if (h >= 10) tft.fillRoundRect(mx-mw/2+3, my, mw-6, min(h/2,8), 3, TFT_WHITE);
        if (h >= 18) tft.fillEllipse(mx, my+h-6, 12, 6, C_TONGUE);
    } else {
        bezier2(mx-30, my, mx, my-20, mx+30, my, 3, C_LIP_DARK, 40);
        bezier2(mx-30, my, mx, my-18, mx+30, my, 2, C_LIP,      40);
        tft.fillCircle(mx-30, my, 4, C_LIP_DARK);
        tft.fillCircle(mx+30, my, 4, C_LIP_DARK);
        tft.fillCircle(mx-30, my, 2, C_LIP);
        tft.fillCircle(mx+30, my, 2, C_LIP);
        bezier2(mx-18, my-12, mx, my-16, mx+18, my-12, 1, C_LIP_HI, 15);
    }
    tft.endWrite();
}

// =====================================================================
// VẼ ĐỒNG HỒ 
// =====================================================================
void drawClock(const String& t) {
    tft.startWrite();
    tft.fillRect(SPLIT_X+1, 191, 319-SPLIT_X, 49, 0x0820);
    tft.drawLine(SPLIT_X+1, 191, 319, 191, C_DIVIDER);
    
    tft.drawCircle(155, 215, 9, C_LABEL);
    tft.drawLine(155, 215, 155, 208, C_LABEL);
    tft.drawLine(155, 215, 161, 215, C_LABEL);
    tft.setCursor(169, 207);
    
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);
    tft.print(t == "" ? "--:--" : t);
    tft.endWrite();
}

// =====================================================================
// VẼ SENSOR PANEL 
// =====================================================================
uint16_t pm25Color(int v) {
    if (v <= 12) return C_PM25_G;
    if (v <= 35) return C_PM25_Y;
    if (v <= 55) return C_PM25_O;
    return C_PM25_R;
}

void drawSensorCard(int y, uint16_t dotColor, const char* label, const String& val, const char* unit, uint16_t valColor) {
    tft.fillRoundRect(5,  y, 130, 52, 6, C_PANEL_BG);
    tft.drawRoundRect(5,  y, 130, 52, 6, C_PANEL_BD);
    tft.fillCircle(19, y+13, 5, dotColor);
    tft.setCursor(28, y+7);
    tft.setTextColor(C_LABEL);
    tft.setTextSize(1);
    tft.print(label);
    tft.setCursor(10, y+22);
    tft.setTextSize(2);
    if (val == "N/A" || val == "" || val == "--") {
        tft.setTextColor(0x4208);
        tft.print("--");
    } else {
        tft.setTextColor(valColor);
        tft.print(val);
    }
    int cx = tft.getCursorX();
    tft.setCursor(cx+2, y+30);
    tft.setTextSize(1);
    tft.setTextColor(C_LABEL);
    tft.print(unit);
}

void drawSensorPanel() {
    tft.startWrite();
    tft.fillRect(0, 0, SPLIT_X, 240, 0x0410);
    tft.setCursor(6, 4);
    tft.setTextColor(0x7BEF);
    tft.setTextSize(1);
    tft.print("AIR QUALITY"); // Đổi lại thành tiêu đề chung
    
    drawSensorCard(16,  pm25Color(g_pm25.toInt()), "PM2.5",    g_pm25, "ug/m3", pm25Color(g_pm25.toInt()));
    drawSensorCard(76,  C_PM10_COL,                "PM10",     g_pm10, "ug/m3", C_PM10_COL);
    drawSensorCard(136, C_TEMP_COL,                "TEMP",     g_temp, "C",     C_TEMP_COL);
    drawSensorCard(196, C_HUM_COL,                 "HUMIDITY", g_hum,  "%",     C_HUM_COL);
    
    tft.drawLine(SPLIT_X,   0, SPLIT_X,   239, C_DIVIDER);
    tft.drawLine(SPLIT_X-1, 0, SPLIT_X-1, 239, 0x1020);
    tft.endWrite();
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    pinMode(BOOT_PIN, INPUT_PULLUP);

    tft.init();
    tft.setRotation(2);   
    tft.setTextFont(0);
    tft.fillScreen(TFT_BLACK);

    tft.setCursor(30, 100);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.print("Connecting WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        Serial.print(".");
        tries++;
    }

    tft.fillScreen(TFT_BLACK);
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi: " + WiFi.localIP().toString());
        tft.setCursor(10, 100);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(1);
        tft.print("WiFi: ");
        tft.print(WiFi.localIP().toString());
        delay(1500);
    } else {
        Serial.println("\n⚠️  WiFi thất bại — Audio sẽ không hoạt động");
        tft.setCursor(10, 100);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.print("WiFi FAILED - No audio");
        delay(2000);
    }

    initES8311();

    pinMode(PA_PIN, OUTPUT);
    digitalWrite(PA_PIN, HIGH);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(18);   

    tft.fillScreen(TFT_BLACK);
    drawSensorPanel();
    drawFace(true, false, 0);
    drawClock("--:--");

    Serial.println("READY");
}

// =====================================================================
// LOOP
// =====================================================================
void loop() {
    audio.loop();

    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        // KHI NHẬN LỆNH ĐỔI PHÒNG -> VẼ LẠI CẢ MẶT (CÓ CHỮ) VÀ CẢM BIẾN
        if (cmd.startsWith("ROOM:"))  { 
            g_room = cmd.substring(5); 
            g_needSensor = true; 
            g_needFace = true; 
        }
        else if (cmd.startsWith("PM25:"))  { g_pm25 = cmd.substring(5); g_needSensor = true; }
        else if (cmd.startsWith("PM10:"))  { g_pm10 = cmd.substring(5); g_needSensor = true; }
        else if (cmd.startsWith("TEMP:"))  { g_temp = cmd.substring(5); g_needSensor = true; }
        else if (cmd.startsWith("HUM:"))   { g_hum  = cmd.substring(5); g_needSensor = true; }
        else if (cmd.startsWith("TIME:"))  { g_time = cmd.substring(5); drawClock(g_time); }
        else if (cmd.startsWith("PLAY:"))  {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("AUDIO_FAIL"); 
            } else {
                String url = cmd.substring(5);
                Serial.println("🌐 Fetching: " + url);
                bool ok = audio.connecttohost(url.c_str());
                if (!ok) {
                    Serial.println("AUDIO_FAIL"); 
                } else {
                    g_isTalking  = true;
                    g_needFace   = true;
                }
            }
        }
        else if (cmd == "TALK")    { g_isTalking = true;  g_eyesOpen = true;  g_needFace = true; }
        else if (cmd == "WAKE_UP") { g_isTalking = false; g_eyesOpen = true;  g_needFace = true; }
        else if (cmd == "SLEEP")   { g_isTalking = false; g_eyesOpen = false; g_needFace = true; }
    }

    // Xử lý nút vật lý để đổi phòng
    bool currentBtnState = digitalRead(BOOT_PIN);
    if (currentBtnState == LOW && lastBtnState == HIGH) {
        delay(50); 
        if (digitalRead(BOOT_PIN) == LOW) {
            Serial.println("BTN_TOGGLE_ROOM"); 
            while(digitalRead(BOOT_PIN) == LOW) { delay(10); } 
        }
    }
    lastBtnState = currentBtnState;

    unsigned long now = millis();

    if (audio.isRunning()) {
        if (now - lastAnimMs >= 120) {
            lastAnimMs   = now;
            g_mouthFrame = (g_mouthFrame + 1) % 4;
            drawFace(true, true, g_mouthFrame);
            g_needFace = false;
        }
    } else if (g_isTalking) {
        g_isTalking = false;
        g_eyesOpen  = true;
        g_needFace  = true;
        Serial.println("AUDIO_DONE");
    }

    if (g_eyesOpen && !g_isTalking) {
        if (now - lastBlinkMs >= blinkInterval) {
            lastBlinkMs   = now;
            blinkInterval = 3000 + random(3000);
            drawFace(false, false, 0);
            delay(110);
            drawFace(true,  false, 0);
            g_needFace = false;
        }
    }

    if (g_needFace) {
        g_needFace = false;
        drawFace(g_eyesOpen, g_isTalking, g_mouthFrame);
    }

    if (g_needSensor) {
        g_needSensor = false;
        drawSensorPanel();
    }
}

// =====================================================================
// AUDIO CALLBACKS
// =====================================================================
void audio_info(const char* info) {
    Serial.print("ℹ️  "); Serial.println(info);
}
void audio_eof_mp3(const char* info) {
    Serial.println("AUDIO_DONE");
}
void audio_showstation(const char* info) {
    Serial.print("Station: "); Serial.println(info);
}