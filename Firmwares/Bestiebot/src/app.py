from flask import Flask, request, jsonify, render_template_string
from pymongo import MongoClient
import datetime

app = Flask(__name__)

# Kết nối MongoDB Atlas của bạn
MONGO_URI = "mongodb+srv://anhtran3122002_db_user:Nhatanh312@cluster0.427mokq.mongodb.net/?retryWrites=true&w=majority"
client = MongoClient(MONGO_URI)
db = client['BestieRobotDB']        
logs_collection = db['chat_logs']  

HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Bestie Robot Dashboard</title>
    <meta http-equiv="refresh" content="5">
    <style>
        body { font-family: sans-serif; margin: 40px; background: #f4f4f9; }
        table { width: 100%; border-collapse: collapse; background: white; }
        th, td { padding: 12px; border: 1px solid #ddd; text-align: left; }
        th { background: #00bcd4; color: white; }
        tr:nth-child(even) { background: #f2f2f2; }
    </style>
</head>
<body>
    <h1>🚀 Bestie Robot Live Dashboard</h1>
    <table>
        <tr>
            <th>Time</th>
            <th>User Input</th>
            <th>Bestie Response</th>
            <th>Light</th>
        </tr>
        {% for log in logs %}
        <tr>
            <td>{{ log.timestamp }}</td>
            <td>{{ log.user_input }}</td>
            <td>{{ log.bot_response }}</td>
            <td>{{ log.light_status }}</td>
        </tr>
        {% endfor %}
    </table>
</body>
</html>
"""

@app.route('/')
def index():
    # Lấy dữ liệu từ MongoDB để hiển thị
    logs = list(logs_collection.find().sort("_id", -1).limit(20))
    return render_template_string(HTML_TEMPLATE, logs=logs)

@app.route('/robot/log', methods=['POST'])
def log_data():
    data = request.json
    if data:
        data['timestamp'] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        logs_collection.insert_one(data)
    return jsonify({"status": "success"}), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001)