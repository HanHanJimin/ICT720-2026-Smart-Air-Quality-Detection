"""
robot_brain.py — Bestie AI Robot (English Version)
Luồng âm thanh:
  Python tạo MP3 (gTTS) → lưu vào /tmp/bestie_audio/response.mp3
  Gửi lệnh "TALK" qua Serial để màn hình ESP32 nhép miệng.
  Phát âm thanh trực tiếp qua loa máy tính (laptop) bằng thư viện pygame.
  Khi phát xong, gửi lệnh "WAKE_UP" để ESP32 dừng nhép miệng.
"""

import google.generativeai as genai
import speech_recognition as sr
from gtts import gTTS
import serial
import serial.tools.list_ports
import time
import threading
import datetime
import requests
import os
from langdetect import detect
from pymongo import MongoClient
from typing import Optional
import pygame

# =====================================================================
# CẤU HÌNH
# =====================================================================
GEMINI_KEY   = "AIzaSyBok8qkS8p8zSUYvA4b2mfMcgRtsUo_YXM"
MONGO_URI    = "mongodb+srv://anhtran3122002_db_user:Nhatanh312@cluster0.427mokq.mongodb.net/?retryWrites=true&w=majority"
DOCKER_URL   = "http://localhost:5001/robot/log"
AUDIO_DIR    = "/tmp/bestie_audio"
AUDIO_FILE   = os.path.join(AUDIO_DIR, "response.mp3")

FIREBASE_URL_R1 = "https://besties-591ee-default-rtdb.asia-southeast1.firebasedatabase.app/room1/logs.json"
FIREBASE_URL_R2 = "https://besties-591ee-default-rtdb.asia-southeast1.firebasedatabase.app/room2/logs.json"

current_ui_room = 1 

os.makedirs(AUDIO_DIR, exist_ok=True)
pygame.mixer.init() 

genai.configure(api_key=GEMINI_KEY)
model = genai.GenerativeModel('gemini-3.1-flash-lite-preview')

try:
    mongo_client    = MongoClient(MONGO_URI, serverSelectionTimeoutMS=3000)
    db              = mongo_client['BestieRobotDB']
    logs_collection = db['chat_logs']
    print("✅ MongoDB OK")
except Exception as e:
    print(f"⚠️  MongoDB: {e}")
    logs_collection = None

# =====================================================================
# SERIAL
# =====================================================================
serial_lock = threading.Lock()

def _find_port() -> Optional[str]:
    import glob
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if any(k in desc for k in ['cp210','ch34','uart','usb serial','esp32']) \
        or any(k in hwid for k in ['10c4:ea60','1a86:55d4','303a']):
            return p.device
    for pat in ['/dev/cu.usbmodem*','/dev/ttyUSB*','/dev/ttyACM*']:
        hits = glob.glob(pat)
        if hits: return hits[0]
    return None

def _connect_serial():
    port = _find_port()
    if not port:
        print("⚠️  Không tìm thấy ESP32 — chạy không có phần cứng")
        return None
    try:
        s = serial.Serial(port, 115200, timeout=1)
        time.sleep(2)
        print(f"✅ Serial: {port}")
        return s
    except Exception as e:
        print(f"❌ Serial: {e}")
        return None

ser = _connect_serial()

def send_cmd(data: bytes):
    if ser and ser.is_open:
        with serial_lock:
            try:
                ser.write(data)
                ser.flush()
            except Exception as e:
                print(f"Serial write error: {e}")

# =====================================================================
# FIREBASE 
# =====================================================================
def fetch_firebase_room(url: str) -> dict:
    out = {"pm25":"N/A","pm10":"N/A","temp":"N/A","humidity":"N/A"}
    try:
        r = requests.get(url, params={'orderBy':'"$key"','limitToLast':1}, timeout=4)
        if r.status_code == 200 and r.json():
            rec = list(r.json().values())[0]
            out["pm25"]     = str(rec.get("pm25") or rec.get("pm2_5") or rec.get("pm") or "N/A")
            out["pm10"]     = str(rec.get("pm10") or rec.get("PM10") or "N/A")
            out["temp"]     = str(rec.get("temp") or rec.get("temperature") or rec.get("t") or "N/A")
            out["humidity"] = str(rec.get("humidity") or rec.get("hum") or rec.get("h") or "N/A")
    except Exception as e:
        print(f"Firebase fetching error: {e}")
    return out

def get_all_sensors() -> dict:
    return {
        1: fetch_firebase_room(FIREBASE_URL_R1),
        2: fetch_firebase_room(FIREBASE_URL_R2)
    }

def push_sensors(data: dict):
    send_cmd(f"PM25:{data['pm25']}\n".encode())
    time.sleep(0.04)
    send_cmd(f"PM10:{data['pm10']}\n".encode())
    time.sleep(0.04)
    send_cmd(f"TEMP:{data['temp']}\n".encode())
    time.sleep(0.04)
    send_cmd(f"HUM:{data['humidity']}\n".encode())

def send_time():
    send_cmd(f"TIME:{datetime.datetime.now().strftime('%H:%M')}\n".encode())

# =====================================================================
# SPEAK
# =====================================================================
def play_local_audio():
    try:
        pygame.mixer.music.load(AUDIO_FILE)
        pygame.mixer.music.play()
        while pygame.mixer.music.get_busy():
            pygame.time.Clock().tick(10)
    except Exception as e:
        print(f"Pygame error: {e}")

def speak(text: str):
    if not text.strip(): return
    try:
        lang = detect(text)
        # Đã ưu tiên fallback về tiếng Anh ('en') thay vì tiếng Việt
        if lang not in ('en','vi'): lang = 'en'
    except:
        lang = 'en'

    try:
        gTTS(text=text, lang=lang, slow=False).save(AUDIO_FILE)
        print(f"🎵 TTS OK ({lang}): {text[:60]}...")
    except Exception as e:
        print(f"gTTS error: {e}")
        return

    send_cmd(b"TALK\n")
    print("🔊 Đang phát loa laptop...")
    play_local_audio()
    send_cmd(b"WAKE_UP\n")

# =====================================================================
# NGHE MIC (Đã chuyển sang tiếng Anh)
# =====================================================================
def listen() -> str:
    r = sr.Recognizer()
    r.energy_threshold         = 300
    r.dynamic_energy_threshold = True
    with sr.Microphone() as src:
        print("\n👂 Listening...")
        r.adjust_for_ambient_noise(src, duration=0.7)
        try:
            audio = r.listen(src, timeout=6, phrase_time_limit=12)
            # CHÚ Ý ĐIỂM NÀY: Đổi sang en-US
            text  = r.recognize_google(audio, language='en-US')
            print(f"🗣️  User: {text}")
            return text
        except sr.WaitTimeoutError:
            return ""
        except sr.UnknownValueError:
            return ""
        except Exception as e:
            print(f"Listen error: {e}")
            return ""

# =====================================================================
# LƯU DATABASE
# =====================================================================
def save_log(user: str, bot: str, sensors: dict, room: int):
    payload = {
        "user_input":   user,
        "bot_response": bot,
        "room_source":  f"Room {room}",
        **sensors,
        "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }
    try: requests.post(DOCKER_URL, json=payload, timeout=2)
    except: pass
    if logs_collection is not None:
        try: logs_collection.insert_one(payload)
        except Exception as e: print(f"MongoDB: {e}")

# =====================================================================
# THREAD: Cập nhật định kỳ
# =====================================================================
def _periodic():
    global current_ui_room
    while True:
        try:
            sensors = get_all_sensors()
            push_sensors(sensors[current_ui_room])
            send_time()
        except Exception as e:
            pass
        time.sleep(30)

# =====================================================================
# MAIN
# =====================================================================
def run():
    global current_ui_room
    
    # Đã dịch toàn bộ Prompt sang tiếng Anh để Gemini tư duy theo tiếng Anh
    SYSTEM = (
        "You are Bestie, a friendly AI robot. Rules:\n"
        "1. Answer concisely in 1-3 sentences, act naturally.\n"
        "2. CRITICAL: You monitor 2 rooms (Room 1 and Room 2). "
        "If the user asks about data (temperature, humidity, air quality) WITHOUT specifying a room, "
        "YOU MUST ASK BACK: 'Do you want data for Room 1 or Room 2?'. "
        "DO NOT INVENT or assume data if the room is unknown.\n"
        "3. SCREEN SWITCH COMMAND (MANDATORY): If the user asks for specific data of Room 1 or Room 2, "
        "you MUST read the data AND APPEND EXACTLY the string [SWITCH_ROOM_1] (for Room 1) "
        "or [SWITCH_ROOM_2] (for Room 2) at the END OF YOUR SENTENCE. "
        "Example: 'The temperature in room 1 is 25 degrees. [SWITCH_ROOM_1]'\n"
        "4. Light control: append [LIGHT_ON] or [LIGHT_OFF] at the end of the sentence if requested."
    )

    sensors = get_all_sensors()
    send_cmd(f"ROOM:{current_ui_room}\n".encode())
    push_sensors(sensors[current_ui_room])
    send_time()

    threading.Thread(target=_periodic, daemon=True).start()

    print("\n🤖 Bestie ready!\n")
    # Đổi câu chào sang tiếng Anh
    speak("Hello! I am Bestie. How can I help you today?")

    while True:
        t_start_listen = time.time()
        user = listen()
        if not user:
            if ser and ser.in_waiting:
                try:
                    line = ser.readline().decode(errors='ignore').strip()
                    if "BTN_TOGGLE_ROOM" in line:
                        current_ui_room = 2 if current_ui_room == 1 else 1
                        send_cmd(f"ROOM:{current_ui_room}\n".encode())
                        s_data = get_all_sensors()
                        push_sensors(s_data[current_ui_room])
                        print(f"🔄 Nút cứng: Đã chuyển sang Room {current_ui_room}")
                except: pass
            continue
        
        print(f"⏱️ [1. Mic] Listening took: {time.time() - t_start_listen:.2f} s")

        sensors = get_all_sensors()

        prompt = (
            f"{SYSTEM}\n\n"
            f"[Current Sensor Data]\n"
            f"Room 1 - PM2.5: {sensors[1]['pm25']}, Temp: {sensors[1]['temp']}°C, Hum: {sensors[1]['humidity']}%\n"
            f"Room 2 - PM2.5: {sensors[2]['pm25']}, Temp: {sensors[2]['temp']}°C, Hum: {sensors[2]['humidity']}%\n\n"
            f"User: {user}"
        )

        try:
            t_start_ai = time.time()
            bot = model.generate_content(prompt).text.strip()
            print(f"⏱️ [2. Gemini] Response time: {time.time() - t_start_ai:.2f} s")
            print(f"🤖 Bestie: {bot}")

            bot_upper = bot.upper()
            if "[SWITCH_ROOM_1]" in bot_upper:
                current_ui_room = 1
                send_cmd(b"ROOM:1\n")
                time.sleep(0.1) 
                push_sensors(sensors[1])
                bot = bot.replace("[SWITCH_ROOM_1]", "").replace("[switch_room_1]", "").strip()
                print("📺 Switched UI -> ROOM 1")
                
            elif "[SWITCH_ROOM_2]" in bot_upper:
                current_ui_room = 2
                send_cmd(b"ROOM:2\n")
                time.sleep(0.1)
                push_sensors(sensors[2])
                bot = bot.replace("[SWITCH_ROOM_2]", "").replace("[switch_room_2]", "").strip()
                print("📺 Switched UI -> ROOM 2")

            if "[LIGHT_ON]" in bot.upper():
                send_cmd(b"LIGHT_ON\n")
                bot = bot.replace("[LIGHT_ON]", "").replace("[light_on]", "").strip()
            elif "[LIGHT_OFF]" in bot.upper():
                send_cmd(b"LIGHT_OFF\n")
                bot = bot.replace("[LIGHT_OFF]", "").replace("[light_off]", "").strip()

            threading.Thread(target=save_log,
                             args=(user, bot, sensors[current_ui_room], current_ui_room),
                             daemon=True).start()
            
            t_start_speak = time.time()
            speak(bot)
            print(f"⏱️ [3. TTS + Speaker] Processing took: {time.time() - t_start_speak:.2f} s\n")

        except Exception as e:
            print(f"❌ System/AI Error: {e}")

if __name__ == "__main__":
    run()