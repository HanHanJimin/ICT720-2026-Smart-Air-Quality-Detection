# ICT720-2026-Smart-Air-Quality-Detection
IoT air quality monitor with AI pollution source detection using ESP32-S2, and Gemini LLM API for ICT720


Repo for demo idea, model, and code for ICT720 course of 2026

## User stories
## Software models
## Software stack

- [AIoT stack](#software-stack)
- [Sequence diagram](#sequence-diagram)

## Software interaction

---

## List of student projects

### Group [GroupName]

#### Members

- **Member 1 Name**: Hardware & Sensor Engineer — Set up and program the ESP32-S2 to read PM2.5 data from the Honeywell sensor via UART, connect to WiFi, and publish air quality readings to the MQTT broker every 5 seconds.
- **Member 2 Name**: Camera & Embedded Engineer — Program the LilyGO T-SimCam ESP32-S3 to subscribe to MQTT alerts, capture a photo when air quality is poor, and send the image to the Python server via HTTP POST.
- **Member 3 Name**: Backend Developer — Build the central Python/FastAPI server in Docker that receives MQTT data, stores readings in MongoDB, checks PM2.5 thresholds, triggers camera alerts, and exposes REST API endpoints for other services.
- **Member 4 Name**: AI / LLM Integration Engineer — Design prompts and integrate the Google Gemini Vision API to analyze camera photos, classify pollution sources (smoke, traffic, construction, cooking), and parse output into structured JSON data.
- **Member 5 Name**: Frontend / Bot Developer — Build the Telegram Bot to send air quality alerts with photos and classifications, and develop the Streamlit web dashboard for real-time monitoring and historical data visualization.

#### Scope

An IoT-based smart air quality monitoring system that uses an ESP32-S2 with a Honeywell PM2.5 sensor to continuously monitor air quality. When PM2.5 levels exceed a safe threshold, the system triggers an ESP32-S3 camera to capture the environment, analyzes the image using an LLM Vision API to identify the pollution source, and alerts users via Telegram with the photo, PM2.5 reading, and identified cause. A web dashboard provides historical data and trends.

---

## User stories

![User Stories](images/user_stories.png)

| As a | I want to | so that |
|------|-----------|---------|
| resident | receive a Telegram alert when PM2.5 exceeds safe levels | I can close windows or wear a mask to protect my health |
| resident | see a photo of what is causing the pollution | I know whether it's smoke, traffic, or construction and can respond |
| resident | set my own PM2.5 alert threshold | I can customize sensitivity based on my health condition |
| building manager | view real-time PM2.5 levels on a web dashboard | I can monitor air quality across the building continuously |
| building manager | view historical PM2.5 data with charts | I can identify patterns and report to management |
| researcher | query collected PM2.5 and image data via REST API | data can be retrieved and used for analysis |
| researcher | review AI classification accuracy on the dashboard | the AI model can be improved over time |

---

## Software stack

![Software Stack](images/software_stack.png)

### Stack details

| # | Stack | Technology | Description |
|---|-------|------------|-------------|
| 1 | IoT Sensor Stack (Embedded) | ESP32-S2, ESP32-S3, Honeywell PM2.5, Arduino, Paho MQTT | Reads air quality data and captures photos |
| 2 | Data Collector Stack (Server/Docker) | Python, FastAPI, Paho MQTT, pymongo, MongoDB | Central server that receives, stores, and processes all data |
| 3 | AI Stack (Cloud API) | Google Gemini Vision API, Prompt Engineering | Classifies pollution source from camera photos |
| 4 | Chatbot Stack (Service/Docker) | Python, pyTelegramBotAPI | Sends alerts to users via Telegram |
| 5 | Dashboard Stack (UI/Docker) | Python, Streamlit, Plotly | Web dashboard for monitoring and history |

---

## Sequence diagram

![Sequence Diagram](images/sequence_diagram.png)

### Phase 1: PM2.5 Data Collection (continuous, every 5 seconds)

```
ESP32-S2 reads PM2.5 via UART → publishes to MQTT broker → 
Server subscribes and receives → parses JSON → stores in MongoDB → checks threshold
```

### Phase 2: Camera Trigger + AI Classification (when PM2.5 > 50 µg/m³)

```
Server publishes "air_bad" alert via MQTT → MQTT forwards to ESP32-S3 →
ESP32-S3 captures photo → HTTP POST to server → 
Server sends photo to Gemini Vision API → AI returns classification →
Server stores result in MongoDB → sends alert to Telegram bot
```

### Phase 3: Dashboard Updates (continuous polling)

```
Streamlit dashboard → queries REST API → receives PM2.5 history + photos + classifications → 
updates charts and displays
```

---

## Hardware

| Device | Model | Purpose |
|--------|-------|---------|
| Microcontroller 1 | ESP32-S2 "Cucumber" | Reads PM2.5 sensor data, publishes via MQTT |
| Microcontroller 2 | LILYGO T-SIMCAM ESP32-S3 (V1.2) | Camera capture, triggered by MQTT alert |
| Sensor | Honeywell HPM PM2.5 (P/N: 32326466-001) | Measures PM2.5 and PM10 air particles |
| Breadboard | Standard full-size solderless | Prototyping connections |

---

## Project structure

```
ict720-smart-air-quality/
├── README.md
├── images/                          ← Diagrams for this page
│   ├── software_stack.png
│   ├── sequence_diagram.png
│   └── user_stories.png
├── docker-compose.yml               ← One command starts all services
├── mosquitto/
│   └── config/mosquitto.conf
├── firmware/
│   ├── esp32s2_pm25/                ← Member 1
│   │   └── esp32s2_pm25.ino
│   └── esp32s3_camera/              ← Member 2
│       └── esp32s3_camera.ino
├── server/                          ← Member 3 + Member 4
│   ├── Dockerfile
│   ├── main.py
│   ├── ai_classifier.py
│   └── requirements.txt
├── telegram_bot/                    ← Member 5
│   ├── Dockerfile
│   ├── bot.py
│   └── requirements.txt
└── dashboard/                       ← Member 5
    ├── Dockerfile
    ├── app.py
    └── requirements.txt
```

---

## How to run

```bash
# 1. Clone the repo
git clone https://github.com/YOUR_USERNAME/ict720-smart-air-quality.git
cd ict720-smart-air-quality

# 2. Set up environment variables
cp env.example .env
# Edit .env with your Gemini API key and Telegram bot token

# 3. Start all server services
docker-compose up -d

# 4. Flash ESP32-S2 firmware (Member 1)
# Open Arduino IDE → firmware/esp32s2_pm25/esp32s2_pm25.ino → Upload

# 5. Flash ESP32-S3 firmware (Member 2)
# Open Arduino IDE → firmware/esp32s3_camera/esp32s3_camera.ino → Upload

# 6. Access the dashboard
# Open browser → http://localhost:8501
```
