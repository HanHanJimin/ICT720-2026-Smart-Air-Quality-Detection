# ============================================
# bot_final_firebase.py — TWO ROOMS VERSION
# ============================================
# Gemini AI only called when user uses "Ask AI" or types a question.
# Room 1, Room 2, Causes, History → NO Gemini call → fast response.
# Auto alert threshold: 20 µg/m³ (for testing)
# Alerts sent per-room (only rooms that exceed threshold)

import time
import threading
from datetime import datetime

import requests as req
import telebot
from telebot import types
import google.generativeai as genai

# ═══════════════════════════════════════
# SETTINGS
# ═══════════════════════════════════════
TELEGRAM_TOKEN = "---"
GEMINI_API_KEY = "-----"

FIREBASE_URL = "https://besties-591ee-default-rtdb.asia-southeast1.firebasedatabase.app"
DEFAULT_THRESHOLD = 50
ALERT_THRESHOLD = 20  #  Lowered for testing

bot = telebot.TeleBot(TELEGRAM_TOKEN)
genai.configure(api_key=GEMINI_API_KEY)
gemini = genai.GenerativeModel("gemini-3-flash-preview")

subscribed_users = {}

SEP = "\u2500" * 28

AQI_INFO_1 = (
    "\U0001F4D6 PM2.5 Air Quality Guide\n"
    + SEP + "\n\n"
    "\U0001F7E2  0 \u2013 25    Good\n"
    "Air quality is satisfactory. No health risk.\n\n"
    "\U0001F7E1  26 \u2013 50   Moderate\n"
    "Acceptable for most. Sensitive groups be cautious.\n\n"
    "\U0001F7E0  51 \u2013 100  Unhealthy (sensitive)\n"
    "Children & elderly should reduce outdoor activity.\n\n"
    "\U0001F534  101 \u2013 150 Unhealthy\n"
    "Everyone may feel effects. Limit outdoor exertion.\n"
)

AQI_INFO_2 = (
    "\U0001F7E3  151 \u2013 200 Very Unhealthy\n"
    "Health alert. Avoid outdoor activity.\n"
    "Close windows. Use air purifier.\n\n"
    "\U0001F7E4  201+      Hazardous\n"
    "Emergency conditions.\n"
    "Stay indoors. Wear N95 mask outside.\n\n"
    + SEP + "\n"
    "Source: WHO Air Quality Guidelines\n"
    "Default alert threshold: 50 \u00b5g/m\u00b3"
)


# ═══════════════════════════════════════
# FIREBASE
# ═══════════════════════════════════════

def read_room(room):
    try:
        url = f'{FIREBASE_URL}/{room}/logs.json?orderBy="$key"&limitToLast=1'
        r = req.get(url, timeout=10)
        data = r.json()
        if data and isinstance(data, dict):
            key = list(data.keys())[0]
            return data[key]
    except Exception as e:
        print(f"[Firebase] {room} error: {e}")
    return None

def read_room_history(room, limit=5):
    try:
        url = f'{FIREBASE_URL}/{room}/logs.json?orderBy="$key"&limitToLast={limit}'
        r = req.get(url, timeout=10)
        data = r.json()
        if data and isinstance(data, dict):
            entries = []
            for key in sorted(data.keys(), reverse=True):
                e = data[key]
                entries.append({
                    "pm25": e.get("pm2_5", 0), "pm10": e.get("pm10", 0),
                    "temp": round(e.get("temp", 0), 1),
                    "humidity": round(e.get("humidity", 0), 1),
                    "time": e.get("time", ""),
                })
            return entries
    except Exception as e:
        print(f"[Firebase] {room} history error: {e}")
    return []


# ═══════════════════════════════════════
# HELPERS
# ═══════════════════════════════════════

def get_level(pm25):
    if pm25 <= 25: return "Good"
    elif pm25 <= 50: return "Moderate"
    elif pm25 <= 100: return "Unhealthy for Sensitive Groups"
    elif pm25 <= 150: return "Unhealthy"
    elif pm25 <= 200: return "Very Unhealthy"
    else: return "Hazardous"

def get_emoji(pm25):
    if pm25 <= 25: return "\U0001F7E2"
    elif pm25 <= 50: return "\U0001F7E1"
    elif pm25 <= 100: return "\U0001F7E0"
    elif pm25 <= 150: return "\U0001F534"
    else: return "\U0001F7E3"

def get_threshold(chat_id):
    return subscribed_users.get(chat_id, {}).get("threshold", DEFAULT_THRESHOLD)

def format_time(t):
    if not t: return "N/A"
    try:
        dt = datetime.fromisoformat(t.replace("Z", "+00:00"))
        return dt.strftime("%d %b %Y, %H:%M")
    except:
        return t[:19] if len(t) >= 19 else t


# ═══════════════════════════════════════
# ✅ SMART STATIC SUGGESTION (PM2.5 + Temp)
# ═══════════════════════════════════════

def get_temp_note(temp):
    """Return a temperature-related tip."""
    if temp >= 38:
        return f"Avoid prolonged exposure in the extreme {temp}\u00b0C heat."
    elif temp >= 35:
        return f"Stay hydrated in the {temp}\u00b0C heat and limit time outdoors."
    elif temp >= 32:
        return f"Stay hydrated in the {temp}\u00b0C heat."
    elif temp >= 28:
        return f"Warm at {temp}\u00b0C \u2014 drink water if outdoors."
    else:
        return f"Temperature is comfortable at {temp}\u00b0C."

def get_suggestion_static(pm25, temp):
    """Return a suggestion message combining PM2.5 level and temperature."""
    temp_note = get_temp_note(temp)

    if pm25 <= 25:
        return (
            f"Air quality is good. Enjoy outdoor activities, but {temp_note.lower()}"
        )
    elif pm25 <= 50:
        return (
            f"Air quality is moderate. Sensitive groups should limit prolonged outdoor activity. {temp_note}"
        )
    elif pm25 <= 100:
        return (
            f"Air is unhealthy for sensitive groups. Reduce outdoor exertion and keep windows closed. {temp_note}"
        )
    elif pm25 <= 150:
        return (
            f"Air quality is unhealthy. Avoid outdoor activities and use an air purifier if available. {temp_note}"
        )
    elif pm25 <= 200:
        return (
            f"Air quality is very unhealthy. Stay indoors, close all windows, and run an air purifier. {temp_note}"
        )
    else:
        return (
            f"Hazardous air quality. Stay indoors at all times. Wear an N95 mask if you must go outside. {temp_note}"
        )


# ═══════════════════════════════════════
# ✅ STATIC CAUSES (based on PM2.5 + time)
# ═══════════════════════════════════════

def get_causes_static(pm25):
    hour = datetime.now().hour
    if 8 <= hour < 10:
        time_ctx = "morning rush hour"
    elif 10 <= hour < 14:
        time_ctx = "midday"
    elif 17 <= hour < 20:
        time_ctx = "evening rush hour"
    elif 5 <= hour < 8:
        time_ctx = "early morning"
    else:
        time_ctx = "off-peak hours"

    if pm25 <= 25:
        return (
            f"1. Minimal sources \u2014 air is clean\n"
            f"2. Light background dust or pollen\n"
            f"3. Good ventilation keeping levels low"
        )
    elif pm25 <= 50:
        return (
            f"1. Light vehicle traffic ({time_ctx})\n"
            f"2. Dust from nearby roads or construction\n"
            f"3. Indoor cooking or ventilation"
        )
    elif pm25 <= 100:
        return (
            f"1. Vehicle traffic emissions ({time_ctx})\n"
            f"2. Open burning or cooking smoke nearby\n"
            f"3. Construction dust or industrial activity"
        )
    elif pm25 <= 150:
        return (
            f"1. Heavy traffic and diesel emissions ({time_ctx})\n"
            f"2. Open burning \u2014 crop or waste fires\n"
            f"3. Industrial smoke or factory emissions"
        )
    else:
        return (
            f"1. Crop or waste burning (dry season)\n"
            f"2. Cross-border haze from regional fires\n"
            f"3. Heavy traffic and diesel emissions ({time_ctx})"
        )


# ═══════════════════════════════════════
# Gemini (Ask AI only)
# ═══════════════════════════════════════

def ask_gemini(prompt):
    for _ in range(2):
        try:
            return gemini.generate_content(prompt).text.strip()
        except Exception as e:
            err = str(e).lower()
            if "429" in err or "quota" in err or "rate" in err:
                time.sleep(5)
            else:
                print(f"[Gemini] {e}")
                return None
    return None


# ═══════════════════════════════════════
# KEYBOARDS
# ═══════════════════════════════════════

def main_menu_kb():
    mk = types.InlineKeyboardMarkup(row_width=2)
    mk.add(
        types.InlineKeyboardButton("\U0001F3E0 Room 1", callback_data="room_room1"),
        types.InlineKeyboardButton("\U0001F3E2 Room 2", callback_data="room_room2"),
        types.InlineKeyboardButton("\U0001F4D6 AQI Guide", callback_data="aqi"),
        types.InlineKeyboardButton("\U0001F4AC Ask AI", callback_data="ask_ai"),
    )
    return mk

def room_kb(room, pm25):
    mk = types.InlineKeyboardMarkup(row_width=2)
    btns = []
    if pm25 > DEFAULT_THRESHOLD:
        btns.append(types.InlineKeyboardButton("\U000026A0 Estimated Causes", callback_data=f"causes_{room}"))
    btns.append(types.InlineKeyboardButton("\U0001F4C3 History (last 5)", callback_data=f"history_{room}"))
    if btns: mk.add(*btns)
    mk.add(types.InlineKeyboardButton("\U00002B05 Back to Main Menu", callback_data="main_menu"))
    return mk

def back_main_kb():
    mk = types.InlineKeyboardMarkup()
    mk.add(types.InlineKeyboardButton("\U00002B05 Back to Main Menu", callback_data="main_menu"))
    return mk

def back_room_kb(room):
    mk = types.InlineKeyboardMarkup(row_width=2)
    label = "Room 1" if room == "room1" else "Room 2"
    mk.add(
        types.InlineKeyboardButton(f"\U00002B05 Back to {label}", callback_data=f"room_{room}"),
        types.InlineKeyboardButton("\U0001F3E0 Main Menu", callback_data="main_menu"),
    )
    return mk


# ═══════════════════════════════════════
# FORMAT ROOM DATA
# ═══════════════════════════════════════

def format_room_message(room, data, chat_id):
    room_label = "Room 1" if room == "room1" else "Room 2"
    room_icon = "\U0001F3E0" if room == "room1" else "\U0001F3E2"
    pm25 = data.get("pm2_5", 0)
    pm10 = data.get("pm10", 0)
    temp = round(data.get("temp", 0), 1)
    humidity = round(data.get("humidity", 0), 1)
    ts = data.get("time", "")
    level = get_level(pm25)
    emoji = get_emoji(pm25)
    threshold = get_threshold(chat_id)

    # ✅ Smart suggestion with PM2.5 + temperature
    suggestion = get_suggestion_static(pm25, temp)

    if pm25 > threshold:
        status = f"\U000026A0 PM2.5 exceeds threshold ({threshold} \u00b5g/m\u00b3)"
    else:
        status = f"\U00002705 PM2.5 within safe range (\u2264{threshold} \u00b5g/m\u00b3)"

    msg = (
        f"{room_icon} {room_label} \u2014 Air Quality\n"
        + SEP + "\n\n"
        + f"{emoji}  PM2.5:    {pm25} \u00b5g/m\u00b3  ({level})\n"
        f"\U0001F32B  PM10:     {pm10} \u00b5g/m\u00b3\n"
        f"\U0001F321  Temp:     {temp}\u00b0C\n"
        f"\U0001F4A7  Humidity: {humidity}%\n"
        f"\U0001F552  Updated:  {format_time(ts)}\n\n"
        f"{status}\n"
        + SEP + "\n\n"
        + f"\U0001F4A1 {suggestion}"
    )
    return msg, pm25


# ═══════════════════════════════════════
# /start and /menu
# ═══════════════════════════════════════

@bot.message_handler(commands=["start", "menu"])
def handle_start(message):
    chat_id = message.chat.id
    if chat_id not in subscribed_users:
        subscribed_users[chat_id] = {"threshold": DEFAULT_THRESHOLD}
    bot.send_message(chat_id,
        "\U0001F32C Smart Air Quality Monitor\n"
        + SEP + "\n\n"
        "Real-time PM2.5 monitoring via IoT sensors.\n\n"
        "\U0001F4CD  Two monitoring locations\n"
        "\U0001F4CA  Live PM2.5, PM10, temp, humidity\n"
        "\U000026A0  Pollution cause estimation\n"
        "\U0001F4D6  AQI education guide\n"
        "\U0001F4AC  Ask AI any air quality question\n\n"
        f"Alert threshold: {get_threshold(chat_id)} \u00b5g/m\u00b3 (WHO)\n\n"
        "Select a room to check air quality:",
        reply_markup=main_menu_kb()
    )

@bot.message_handler(commands=["aqi"])
def cmd_aqi(message):
    bot.send_message(message.chat.id, AQI_INFO_1)
    bot.send_message(message.chat.id, AQI_INFO_2, reply_markup=back_main_kb())


# ═══════════════════════════════════════
# BUTTON HANDLER
# ═══════════════════════════════════════

@bot.callback_query_handler(func=lambda call: True)
def handle_callback(call):
    chat_id = call.message.chat.id
    data = call.data
    bot.answer_callback_query(call.id)

    if data == "main_menu":
        bot.send_message(chat_id,
            "\U0001F32C Air Quality Monitor\n\nSelect a room:",
            reply_markup=main_menu_kb())

    elif data.startswith("room_"):
        room = data.replace("room_", "")
        bot.send_chat_action(chat_id, "typing")
        entry = read_room(room)
        if entry is None:
            bot.send_message(chat_id,
                f"\U0000274C Cannot read data from {room}.\nSensor may be offline.",
                reply_markup=back_main_kb())
            return
        msg, pm25 = format_room_message(room, entry, chat_id)
        bot.send_message(chat_id, msg, reply_markup=room_kb(room, pm25))

    elif data.startswith("causes_"):
        room = data.replace("causes_", "")
        bot.send_chat_action(chat_id, "typing")
        entry = read_room(room)
        if entry is None:
            bot.send_message(chat_id, "Cannot read sensor data.", reply_markup=back_room_kb(room))
            return
        pm25 = entry.get("pm2_5", 0)
        pm10 = entry.get("pm10", 0)
        temp = round(entry.get("temp", 0), 1)
        humidity = round(entry.get("humidity", 0), 1)
        room_label = "Room 1" if room == "room1" else "Room 2"
        causes = get_causes_static(pm25)
        bot.send_message(chat_id,
            f"\U000026A0 Pollution Analysis \u2014 {room_label}\n"
            + SEP + "\n\n"
            + f"PM2.5: {pm25} \u00b5g/m\u00b3 {get_emoji(pm25)} ({get_level(pm25)})\n"
            f"PM10:  {pm10} \u00b5g/m\u00b3\n"
            f"Temp:  {temp}\u00b0C | Humidity: {humidity}%\n\n"
            "Estimated causes:\n"
            + SEP + "\n\n"
            + f"{causes}\n\n"
            + SEP + "\n"
            "\U00002139 Based on PM2.5 level and time of day.\n"
            "For AI analysis, use \U0001F4AC Ask AI.",
            reply_markup=back_room_kb(room))

    elif data.startswith("history_"):
        room = data.replace("history_", "")
        bot.send_chat_action(chat_id, "typing")
        entries = read_room_history(room, 5)
        room_label = "Room 1" if room == "room1" else "Room 2"
        threshold = get_threshold(chat_id)
        if not entries:
            bot.send_message(chat_id, f"No data for {room_label}.", reply_markup=back_room_kb(room))
            return
        text = f"\U0001F4C3 Recent Readings \u2014 {room_label}\n" + SEP + "\n\n"
        for i, e in enumerate(entries, 1):
            pm25 = e["pm25"]
            alert = "  \U000026A0" if pm25 > threshold else ""
            text += (f"{i}.  {get_emoji(pm25)} PM2.5: {pm25} \u00b5g/m\u00b3{alert}\n"
                     f"     PM10: {e['pm10']} | {e['temp']}\u00b0C | {e['humidity']}%\n"
                     f"     {format_time(e['time'])}\n\n")
        text += SEP + f"\n\U000026A0 = exceeded threshold ({threshold} \u00b5g/m\u00b3)"
        bot.send_message(chat_id, text, reply_markup=back_room_kb(room))

    elif data == "aqi":
        bot.send_message(chat_id, AQI_INFO_1)
        bot.send_message(chat_id, AQI_INFO_2, reply_markup=back_main_kb())

    elif data == "ask_ai":
        bot.send_message(chat_id,
            "\U0001F4AC Ask AI about air quality\n"
            + SEP + "\n\n"
            "Type your question and I'll answer\n"
            "using live sensor data + AI.\n\n"
            "Examples:\n"
            "\u2022 Is it safe to exercise outside?\n"
            "\u2022 Should I wear a mask today?\n"
            "\u2022 What does PM2.5 mean?\n"
            "\u2022 How can I improve indoor air?\n"
            "\u2022 Is it safe to open windows?",
            reply_markup=back_main_kb())


# ═══════════════════════════════════════
# FREE TEXT → Ask AI (only Gemini call)
# ═══════════════════════════════════════

@bot.message_handler(func=lambda message: True)
def handle_text(message):
    text = message.text.strip()
    if len(text) < 3:
        bot.reply_to(message, "Type a question or send /menu")
        return
    chat_id = message.chat.id
    bot.send_chat_action(chat_id, "typing")
    r1 = read_room("room1")
    r2 = read_room("room2")
    ctx = []
    if r1: ctx.append(f"Room 1: PM2.5={r1.get('pm2_5',0)}, PM10={r1.get('pm10',0)}, temp={r1.get('temp',0)}C, humidity={r1.get('humidity',0)}%")
    if r2: ctx.append(f"Room 2: PM2.5={r2.get('pm2_5',0)}, PM10={r2.get('pm10',0)}, temp={r2.get('temp',0)}C, humidity={r2.get('humidity',0)}%")
    sensor_ctx = "\n".join(ctx) if ctx else "No sensor data."
    answer = ask_gemini(
        f"Professional air quality advisor in Bangkok.\n"
        f"Sensor data:\n{sensor_ctx}\n\n"
        f"User question: \"{text}\"\n\n"
        f"Give a helpful, friendly answer. Reference sensor readings if relevant.\n"
        f"If unrelated to air quality, politely redirect.\n"
        f"Max 50 words."
    )
    if answer is None:
        answer = "Unable to process right now. Please try again later."
    summary = ""
    if r1: summary += f"\U0001F3E0 Room 1: PM2.5 = {r1.get('pm2_5',0)} {get_emoji(r1.get('pm2_5',0))}\n"
    if r2: summary += f"\U0001F3E2 Room 2: PM2.5 = {r2.get('pm2_5',0)} {get_emoji(r2.get('pm2_5',0))}\n"
    bot.send_message(chat_id, f"{summary}\n{answer}", reply_markup=back_main_kb())


# ═══════════════════════════════════════
# ✅ AUTO ALERT — per room, threshold = 20
# ═══════════════════════════════════════

def auto_alert_loop():
    # Track last alert time per room separately
    last_alert = {"room1": 0, "room2": 0}
    print(f"[Alert] Monitoring both rooms every 2 min... threshold={ALERT_THRESHOLD} µg/m³\n")

    while True:
        try:
            now = time.time()
            alert_rooms = []  # ✅ collect rooms that exceed threshold this cycle

            for room in ["room1", "room2"]:
                # Cooldown: don't re-alert same room within 3 min
                if now - last_alert[room] < 180:
                    continue
                entry = read_room(room)
                if entry is None:
                    continue
                pm25 = entry.get("pm2_5", 0)
                if pm25 > ALERT_THRESHOLD:
                    alert_rooms.append((room, entry))

            # ✅ Send alert only for rooms that exceeded threshold
            if alert_rooms:
                for chat_id in list(subscribed_users.keys()):
                    for room, entry in alert_rooms:
                        pm25 = entry.get("pm2_5", 0)
                        pm10 = entry.get("pm10", 0)
                        temp = round(entry.get("temp", 0), 1)
                        humidity = round(entry.get("humidity", 0), 1)
                        ts = entry.get("time", "")
                        room_label = "Room 1" if room == "room1" else "Room 2"
                        room_icon = "\U0001F3E0" if room == "room1" else "\U0001F3E2"

                        # ✅ Static tip using PM2.5 + temp
                        alert_tip = get_suggestion_static(pm25, temp)

                        try:
                            bot.send_message(chat_id,
                                f"\U0001F6A8 Air Quality Alert\n"
                                + SEP + "\n\n"
                                + f"{room_icon}  {room_label}\n\n"
                                f"{get_emoji(pm25)}  PM2.5:  {pm25} \u00b5g/m\u00b3  ({get_level(pm25)})\n"
                                f"\U0001F32B  PM10:   {pm10} \u00b5g/m\u00b3\n"
                                f"\U0001F321  Temp:   {temp}\u00b0C\n"
                                f"\U0001F4A7  Humid:  {humidity}%\n"
                                f"\U0001F552  Time:   {format_time(ts)}\n\n"
                                f"Threshold: {ALERT_THRESHOLD} \u00b5g/m\u00b3\n\n"
                                + SEP + "\n\n"
                                + f"\U0001F4A1 {alert_tip}",
                                reply_markup=room_kb(room, pm25))
                            print(f"[Alert] {room_label}: PM2.5={pm25}>{ALERT_THRESHOLD} -> user {chat_id}")
                        except Exception as e:
                            print(f"[Alert] Send failed: {e}")

                # Update last alert time for alerted rooms
                for room, _ in alert_rooms:
                    last_alert[room] = now

        except Exception as e:
            print(f"[Alert] Error: {e}")
        time.sleep(120)


# ═══════════════════════════════════════
# START
# ═══════════════════════════════════════

if __name__ == "__main__":
    print("=" * 50)
    print("  Smart Air Quality Monitor")
    print("  Firebase + Gemini AI | Two Rooms")
    print("=" * 50)
    for room in ["room1", "room2"]:
        label = "Room 1" if room == "room1" else "Room 2"
        print(f"\n{label}...", end=" ")
        entry = read_room(room)
        if entry:
            print("OK")
            print(f"  PM2.5:    {entry.get('pm2_5', '?')} \u00b5g/m\u00b3 {get_emoji(entry.get('pm2_5', 0))}")
            print(f"  PM10:     {entry.get('pm10', '?')} \u00b5g/m\u00b3")
            print(f"  Temp:     {entry.get('temp', '?')}\u00b0C")
            print(f"  Humidity: {entry.get('humidity', '?')}%")
        else:
            print("OFFLINE")
    print("\nGemini AI...", end=" ")
    try:
        r = gemini.generate_content("Say 'ok' in one word.")
        print(f"OK ({r.text.strip()})")
    except Exception as e:
        print(f"FAILED: {e}")
    print(f"\n{'─' * 50}")
    print(f"Display threshold:  {DEFAULT_THRESHOLD} µg/m³ (WHO)")
    print(f"Alert threshold:    {ALERT_THRESHOLD} µg/m³  ← lowered for testing")
    print("Gemini AI:          Ask AI / free text only")
    print("Room/Causes/History: instant, no AI call")
    print(f"{'─' * 50}")
    print("\nBot running! Open Telegram \u2192 /start")
    print("Press Ctrl+C to stop.\n")
    threading.Thread(target=auto_alert_loop, daemon=True).start()
    try:
        bot.infinity_polling()
    except KeyboardInterrupt:
        print("\nBot stopped.")
