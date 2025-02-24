import base64
from flask import Flask, render_template, jsonify, Response
import paho.mqtt.client as mqtt
import threading
import cv2
import numpy as np
import queue
import os
import time

# Các thông số MQTT
MQTT_BROKER = "192.168.0.102"  # Địa chỉ của ESP32
MQTT_PORT = 1883
MQTT_TOPIC = "Cuong_CC"  # Địa chỉ topic để nhận dữ liệu hình ảnh từ ESP32

# Hàng đợi lưu dữ liệu ảnh
frame_queue = queue.Queue(maxsize=5)  # Giới hạn tối đa là 5 ảnh trong hàng đợi
latest_frame = None  # Lưu frame mới nhất

# Định nghĩa thư mục lưu ảnh
CAPTURE_FOLDER = "static/captured_images"
os.makedirs(CAPTURE_FOLDER, exist_ok=True)

def on_connect(client, userdata, flags, rc, properties=None):
    print(f"Connected with result code {rc}")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    global latest_frame
    message = msg.payload.decode()

    # Kiểm tra nếu là hình ảnh Base64
    if message.startswith('data:image/jpeg;base64,'):
        try:
            image_data = message.split(',', 1)[1]
            image_bytes = base64.b64decode(image_data)
            nparr = np.frombuffer(image_bytes, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            if frame is not None:
                if frame_queue.full():
                    frame_queue.get()  # Xóa frame cũ nhất trong hàng đợi
                frame_queue.put(frame)  # Thêm frame mới vào hàng đợi
        except Exception as e:
            print(f"Error decoding image: {e}")

def generate():
    while True:
        if not frame_queue.empty():
            latest_frame = frame_queue.get()
            _, buffer = cv2.imencode('.jpg', latest_frame, [cv2.IMWRITE_JPEG_QUALITY, 80])
            yield (b'--frame\r\n' b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/capture')
def capture():
    global latest_frame
    if not frame_queue.empty():
        latest_frame = frame_queue.get()
        img_path = os.path.join(CAPTURE_FOLDER, f"captured_{int(time.time())}.jpg")
        cv2.imwrite(img_path, latest_frame)
        return jsonify({"status": "success", "image": f"/{img_path}"})
    return jsonify({"status": "error", "message": "No frame available"})

def mqtt_loop():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

if __name__ == "__main__":
    mqtt_thread = threading.Thread(target=mqtt_loop)
    mqtt_thread.daemon = True
    mqtt_thread.start()
    app.run(host="127.0.0.1", port=5050, debug=True, threaded=True)
