# ICT720-2026-Smart-Air-Quality-Detection
<p>This project designed to help people monitor PM2.5 levels more meaningfully in their own indoor environment. For people who do not own a PM2.5 filter or purifier, it is often difficult to access PM2.5 information directly in their room or workspace. Even for those who already have a PM2.5 filter, most devices only show the current value, without providing any history, trends, or deeper insight. In addition, official air-quality websites usually provide data based on general monitoring stations, so the PM2.5 values may not accurately reflect the actual conditions at your specific location.</p>

**Our Goal:** Help residents monitor and understand air quality through a natural voice interface, providing real-time spoken health advice powered by AI.

## Team Roles

| Name | Role |
|------|------|
| Jesdakorn Jaraschotesathien | IoT Hardware Engineer |
| Nhat Anh Tran | Voice AI Engineer |
| Thinn Thinn Htet | Backend Developer |
| Khin Su Su Han | Integration Developer |
| Napat Charoenwong | Frontend Developer |

---

## 📑 Table of Contents
1. [Scope and Objectives](#1-scope-and-objectives)
2. [User Stories](#2-user-stories)
3. [System Architecture](#3-system-architecture)
4. [Software Stack](#4-software-stack)
5. [Dataflow Diagram](#5-dataflow-diagram)
6. [Tools and Technologies](#6-tools-and-technologies)
7. [Project Structure](#7-project-structure)
8. [Implementation](#8-implementation)
9. [Required Keys](#9-required-keys)
10. [Demo](#10-demo)
11. [Future Work](#11-future-work)
12. [Role and Tasks](#12-role-and-tasks)

---
## 1. Scope and Objectives
An IoT-based smart air quality monitoring system that:
* **Real-time Monitoring:** Uses ESP32-S2 to continuously push PM2.5, PM10, Humidity, and Temperature to Firebase.
* **Interactive AI Voice Assistant:** Uses an **ESP32-S3 (LilyGO T-SimCam)** to listen to user voice commands (e.g., "How is the air today?").
* **Multi-Modal Feedback:** Responds to user queries by fetching live cloud data and displaying it on the **ESP32-S3 built-in screen** while simultaneously answering via **Voice/Speaker**.
* **Visual Intelligence:** Triggers the S3 camera to identify pollution sources using **Google Gemini Vision** when thresholds are exceeded.
* **Dual-Interface Alerts:** Sends detailed health advice and photos to a **Telegram Bot** and logs trends on a **Streamlit Dashboard**.

---

## 2. User Stories
<p>1. As a room owner, I want to monitor PM2.5 levels in my room remotely through a dashboard, so that I can check air quality even when I am not physically there</p>

<p>2. As a room owner, I want to see historical PM2.5 data , so that I can evaluate whether the room environment is improving or getting worse over time.</p>

<p>3. As a room owner, I want to set safe threshold values for PM2.5, so that the system can notify users when the air quality becomes unhealthy.</p>

<p>4. As a room owner, I want to identify time periods when pollution is highest, so that I can improve ventilation or change room usage habits.</p>

![User Stories](images/user_stories.png)

| As a | I want to | so that |
|------|-----------|---------|
| resident | receive a Telegram alert when PM2.5 exceeds safe levels | I can close windows or wear a mask to protect my health |
| resident | see a photo of what is causing the pollution | I know whether it's smoke, traffic, or construction and can respond |
| resident | set my own PM2.5 alert threshold | I can customize sensitivity based on my health condition |
| building manager | view real-time PM2.5 levels on a web dashboard | I can Announce that residents should wear masks or provide health care advice to residents. |
| building manager | view historical PM2.5 data with charts |  I can prepare the monthly air quality report for the juristic committee and find solutions. |
| researcher | query collected PM2.5 and image data via REST API | I can run statistical analysis and build predictive models for pollution forecasting. |
| researcher | review AI classification accuracy on the dashboard | the AI model can be improved over time |

---

## 3. System Architecture

- [AIoT stack](#software-stack)

---

## 4. Software Stack
* **Languages:** C++ (Arduino/Firmware), Python (Backend/AI).
* **Databases:** Firebase Realtime DB (Live), MongoDB (Historical).
* **Backend:** FastAPI (Dockerized).
* **AI:** Google Gemini Vision & Lyria.
* **Frontend:** Streamlit & Telegram Bot API.

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


## 5. Dataflow Diagram

- [Sequence diagram](#sequence-diagram)
- ![Sequence Diagram](images/sequence_diagram.png)


### Phase 1: PM2.5 Data Collection (continuous, every 5 seconds)

```
Server publishes "air_bad" alert via MQTT
→ MQTT forwards to ESP32-S3
→ ESP32-S3 displays current PM2.5 value on screen
→ ESP32-S3 triggers buzzer / LED warning
→ Server sends alert message to Telegram bot
```

### Phase 2: Threshold Alert + Local Display (when PM2.5 > 50 µg/m³)

```
Server publishes "air_bad" alert via MQTT
→ MQTT forwards to ESP32-S3
→ ESP32-S3 displays current PM2.5 value on screen
→ ESP32-S3 triggers buzzer / LED warning
→ Server sends alert message to Telegram bot
```
### Phase 3: Multilingual Voice Query (on-demand by user)

```
User speaks question in any language to ESP32-S3
→ ESP32-S3 captures audio → sends to server via HTTP POST
→ Server calls LLM API (e.g. Gemini) with PM2.5 context + user question
→ LLM generates response in user's language
→ Server returns text response to ESP32-S3
→ ESP32-S3 displays answer on screen (and/or speaks via speaker)
```

### Phase 4: Dashboard Updates

```
Streamlit dashboard
→ queries REST API
→ receives PM2.5 history + alert logs
→ updates charts and displays
```

---

## 6. Tools and Technologies

| Device | Model | Purpose |
|--------|-------|---------|
| Microcontroller 1 | ESP32-S2 "Cucumber" | Reads PM2.5 sensor data, publishes via MQTT |
| Microcontroller 2 | LILYGO T-SIMCAM ESP32-S3 (V1.2) | Receives MQTT alert, displays PM2.5 on built-in LCD, accepts voice input, responds via speaker, triggers buzzer/LED |
| Sensor | Honeywell HPM PM2.5 (P/N: 32326466-001) | Measures PM2.5 and PM10 air particles |
| Breadboard | Standard full-size solderless | Prototyping connections |

---

## 7. Project Structure

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

## 8. Implementation

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

---

## 9. Required Keys

To deploy this ecosystem, you must configure a `.env` file in the root directory with the following credentials:

| Key | Source | Purpose |
| :--- | :--- | :--- |
| `FIREBASE_API_KEY` | Firebase Console | Authentication for ESP32 and Python Client |
| `DATABASE_URL` | Firebase Realtime DB | The REST endpoint for data storage |
| `GEMINI_API_KEY` | Google AI Studio | Powers the Vision and Voice AI analysis |
| `TELEGRAM_BOT_TOKEN` | @BotFather | Enables the Alert Bot to send messages |
| `CHAT_ID` | Telegram | The specific group/user ID for emergency alerts |

---

## 10. Demo

### 🚀 Quick Start for Evaluation:
1.  **Live Stream:** Open the [Firebase Console](https://console.firebase.google.com/) to see the JSON tree updating in real-time as the ESP32-S2 sends packets.
2.  **Threshold Test:** * Expose the sensor to a high-concentration source (or manually update the Firebase value to `> 50`).
    * Observe the **Telegram Bot** instantly pushing a notification with the current PM2.5 value and a timestamp.
3.  **Analytics:** Run the Streamlit dashboard to view the **39+ historical rows** currently stored in the logs, visualized as a trend line.

---

## 11. Future Work

* **Predictive AI:** Implementing a Long Short-Term Memory (LSTM) model to predict air quality spikes 30 minutes in advance.
* **Localized Feedback:** Using the ESP32-S3's built-in LCD to show QR codes that link directly to health advice based on current PM2.5 levels.
* **Advanced Networking:** Transitioning from the Firebase REST API to **MQTT over WebSockets** to reduce battery consumption on the hardware side.

---

## 12. Role and Tasks

Our team divided the project into **4 Functional Pillars** to ensure parallel development during the 48-hour sprint:

### 🛠️ Part 1: Sensory & Edge (Member 1 & 2)
* Hardware wiring and sensor calibration.
* Firmware development for ESP32-S2/S3.
* NTP time synchronization and WiFi stability management.

### ☁️ Part 2: Cloud Infrastructure (Member 3)
* Firebase Realtime Database schema design.
* Backend API development using FastAPI.
* Data validation and "Server-side" timestamping.

### 🤖 Part 3: Intelligence Layer (Member 4)
* Integration of Google Gemini Vision API.
* Prompt engineering for "Health Advice" generation.
* Voice-to-Text and Text-to-Voice processing logic.

### 📊 Part 4: User Experience (Member 5)
* Streamlit Dashboard UI/UX design.
* Telegram Bot event-handling (polling/webhooks).
* Historical data visualization and reporting.


