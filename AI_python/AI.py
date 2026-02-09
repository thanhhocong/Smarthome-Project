import cv2
import numpy as np
import json
import time

from sklearn.ensemble import IsolationForest
from joblib import load
from paho.mqtt import client as mqtt

# ================== CẤU HÌNH ==================
IMG_SIZE = (64, 64)

BROKER = "___YOUR IPv4 ADRESS___FOR EXAMPLE___192.168.120.250"   
PORT   = 1883

STATE_TOPIC          = "ai/face_state"
DOOR_AI_TOPIC        = "home/door/ai"
CAMERA_CONTROL_TOPIC = "ai/camera_control"   # nhận lệnh từ mqtt_sub

MAX_FAIL_ATTEMPTS = 10      # quá 10 lần sai -> tắt cam

CHECK_INTERVAL = 1.0        # GIÃN CÁCH mỗi lần xét đúng/sai (giây)
# ví dụ:
#   0.5  = 2 lần xét / giây
#   1.0  = 1 lần xét / giây
#   2.0  = 1 lần xét / 2 giây

# ================== BIẾN TRẠNG THÁI ==================
camera_on      = False        # ban đầu FLOATING, không mở camera
current_status = "FLOATING"
fail_attempts  = 0
last_check_time = 0.0         # lần cuối cùng thực sự xét đúng/sai

# ================== MQTT CALLBACKS ==================
def on_connect(client, userdata, flags, rc):
    print("MQTT connected, rc =", rc)
    client.subscribe(CAMERA_CONTROL_TOPIC)


def on_message(client, userdata, msg):
    global camera_on, current_status, fail_attempts, last_check_time

    try:
        data = json.loads(msg.payload.decode("utf-8"))
    except Exception as e:
        print("[MQTT RX] JSON lỗi:", e, msg.payload)
        return

    if msg.topic == CAMERA_CONTROL_TOPIC:
        cam = data.get("camera_on")
        if isinstance(cam, bool):
            camera_on = cam
        elif isinstance(cam, str):
            camera_on = (cam.lower() == "true")
        else:
            return

        print("[AI] camera_on =", camera_on)
        if camera_on:
            current_status = "WAIT_FACE"
            fail_attempts = 0
            # reset timer để lần kiểm tra đầu tiên diễn ra ngay lập tức
            last_check_time = 0.0
        else:
            current_status = "FLOATING"

        send_state_telemetry(client)


def send_state_telemetry(client):
    payload = {
        "camera_on": camera_on,
        "status": current_status,
        "fail_attempts": fail_attempts,
    }
    client.publish(STATE_TOPIC, json.dumps(payload))
    print("[STATE]", payload)

# ================== LOAD MODEL ==================
model: IsolationForest = load("face_isoforest.joblib")

face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
)

cap = cv2.VideoCapture(0)

def face_to_vector(gray, x, y, w, h):
    face_img = gray[y:y + h, x:x + w]
    face_img = cv2.resize(face_img, IMG_SIZE)
    vec = face_img.flatten().astype("float32") / 255.0
    return vec

# ================== VÒNG LẶP CHÍNH ==================
def main():
    global current_status, fail_attempts, camera_on, last_check_time

    client = mqtt.Client(client_id="PYTHON_FACE_AI")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    client.loop_start()

    last_status_text = None

    while True:
        # ================== CAMERA OFF MODE ==================
        if not camera_on:
            # Nếu cửa sổ camera đang mở thì đóng hẳn
            try:
                if cv2.getWindowProperty("Face AI", cv2.WND_PROP_VISIBLE) >= 1:
                    cv2.destroyWindow("Face AI")
            except cv2.error:
                pass

            time.sleep(0.1)
            continue

        # ================== CAMERA ON MODE ==================
        ret, frame = cap.read()
        if not ret:
            print("Không đọc được camera")
            time.sleep(0.5)
            continue

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, 1.1, 5)

        status_text = "Dang doi lan kiem tra..."
        color = (0, 255, 255)
        state = "WAIT"

        now = time.time()
        do_check = (now - last_check_time >= CHECK_INTERVAL)

        if do_check:
            # === THỰC SỰ XÉT ĐÚNG/SAI CHỈ KHI ĐỦ KHOẢNG CÁCH THỜI GIAN ===
            status_text = "Không thấy mặt"
            color = (0, 255, 255)
            state = "NO_FACE"

            if len(faces) > 0:
                (x0, y0, w0, h0) = faces[0]

                # khung mở rộng
                pad = int(w0 * 0.25)
                x = max(0, x0 - pad)
                y = max(0, y0 - pad)
                w = w0 + pad * 2
                h = h0 + pad * 2

                face_vec = face_to_vector(gray, x, y, w, h)
                pred = model.predict([face_vec])[0]

                if pred == 1:
                    status_text = "ĐÚNG - Chủ nhà"
                    color = (0, 255, 0)
                    state = "OWNER"

                    cmd = {"cmd": "OPEN_AI"}
                    client.publish(DOOR_AI_TOPIC, json.dumps(cmd))
                    print("[DOOR AI] Publish:", cmd)

                    # TẮT CAMERA NGAY SAU KHI MỞ CỬA
                    camera_on = False
                    current_status = "FLOATING"
                    send_state_telemetry(client)

                    # Đóng cửa sổ camera
                    try:
                        if cv2.getWindowProperty("Face AI", cv2.WND_PROP_VISIBLE) >= 1:
                            cv2.destroyWindow("Face AI")
                            print("[AI] Camera OFF → đóng cửa sổ Face AI")
                    except cv2.error:
                        pass

                    # không cần update last_check_time nữa, vì tắt cam luôn
                    continue

                else:
                    status_text = "SAI - Người lạ"
                    color = (0, 0, 255)
                    state = "OTHER"
                    fail_attempts += 1
                    print(f"[AI] Sai lan thu {fail_attempts}/{MAX_FAIL_ATTEMPTS}")

                    if fail_attempts >= MAX_FAIL_ATTEMPTS:
                        print("[AI] Sai quá nhiều lần → tắt camera")
                        camera_on = False
                        current_status = "FLOATING"
                        send_state_telemetry(client)
                        fail_attempts = 0

                        try:
                            if cv2.getWindowProperty("Face AI", cv2.WND_PROP_VISIBLE) >= 1:
                                cv2.destroyWindow("Face AI")
                        except cv2.error:
                            pass

                        continue

            # cập nhật thời điểm kiểm tra
            last_check_time = now

        # ===== VẼ KHUNG NẾU CÓ MẶT (vẫn vẽ mỗi frame cho đẹp) =====
        if len(faces) > 0:
            (x0, y0, w0, h0) = faces[0]
            pad = int(w0 * 0.25)
            x = max(0, x0 - pad)
            y = max(0, y0 - pad)
            w = w0 + pad * 2
            h = h0 + pad * 2
            cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)

        cv2.putText(
            frame,
            status_text,
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            color,
            2
        )

        cv2.imshow("Face AI", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
