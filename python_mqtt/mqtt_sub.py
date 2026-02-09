import json
from paho.mqtt import client as mqtt
from decrypt import aes_decrypt_from_base64   # dùng lại hàm có sẵn

# ====== BROKER NỘI BỘ (nhận dữ liệu từ 2 Yolo) ======
EDGE_BROKER   = "___YOUR IPv4 ADRESS___FOR EXAMPLE___192.168.120.250" 
EDGE_TOPIC    = "home/nodes/#"     # home/nodes/node1, node2...
CAMERA_CTRL_TOPIC = "ai/camera_control"  # topic điều khiển AI.py

# ====== CORE IOT (giống trong main.cpp) ======
CORE_BROKER   = "app.coreiot.io"
CORE_PORT     = 1883
CORE_TOKEN    = "A9o87CucucvVNeprWJ3h"   # ACCESS_TOKEN_CORE_IOT
CORE_RPC_TOPIC = "v1/devices/me/rpc/request/+"

# client subscribe 
sub_client = mqtt.Client(client_id="PY_BRIDGE_EDGE_SUB")

# client kết nối Core IoT (gửi telemetry + nhận RPC)
core_client = mqtt.Client(client_id="PY_BRIDGE_CORE")


# ========== EDGE side: dữ liệu từ node1/node2 ==========
def on_sub_connect(client, userdata, flags, rc):
    print("EDGE SUB connected to broker", EDGE_BROKER, "rc =", rc)
    client.subscribe(EDGE_TOPIC)
    print("Subscribed to", EDGE_TOPIC)


def on_sub_message(client, userdata, msg):
    payload_str = msg.payload.decode("utf-8", errors="ignore")
    print("\n=== RAW FROM EDGE ===")
    print("Topic:", msg.topic)
    print("Payload:", payload_str)

    try:
        data = json.loads(payload_str)
    except Exception as e:
        print("Không parse được JSON:", e)
        return

    dev = data.get("dev", "unknown")

    if "cipher" not in data:
        print("Không thấy field 'cipher' – bỏ qua.")
        return

    try:
        plain = aes_decrypt_from_base64(data["cipher"])
        print("=== DECRYPTED JSON ===")
        print(plain)

        obj = json.loads(plain)
        obj.setdefault("dev", dev)

        out = json.dumps(obj)
        core_client.publish("v1/devices/me/telemetry", out, qos=1)
        print(">>> Forwarded to Core IoT:", out)

    except Exception as e:
        print("Lỗi giải mã hoặc gửi lên Core IoT:", e)


# ========== CORE side: nhận RPC từ Core IoT ==========
def on_core_connect(client, userdata, flags, rc):
    print("CORE connected to", CORE_BROKER, "rc =", rc)
    # Đăng ký RPC từ Core IoT
    client.subscribe(CORE_RPC_TOPIC)
    print("Subscribed RPC topic:", CORE_RPC_TOPIC)


# mqtt_sub.py
def on_core_message(client, userdata, msg):
    payload_str = msg.payload.decode("utf-8", errors="ignore")
    print("\n=== RPC FROM CORE ===")
    print("Topic:", msg.topic)
    print("Payload:", payload_str)

    try:
        data = json.loads(payload_str)
    except Exception as e:
        print("RPC JSON lỗi:", e)
        return

    method = data.get("method", "")
    params = data.get("params")
    print("RPC method =", method, "params =", params)

    # ==== CAMERA ON/OFF (đã có) ====
    if method == "CamOn":
        cam_on = True
        if isinstance(params, bool):
            cam_on = params
        elif isinstance(params, str):
            cam_on = params.lower() == "true"

        ctrl = {"camera_on": cam_on}
        sub_client.publish(CAMERA_CTRL_TOPIC, json.dumps(ctrl))
        print(">>> Forward camera control to EDGE:", ctrl)

    # ==== GARAGE DOOR (ultrasonic servo) ====
    elif method == "garage_force":
        # mở gara, thời gian sẽ do Node2 quyết định (ví dụ 10s)
        sub_client.publish("home/door/garage", "OPEN", qos=1)
        print(">>> Garage force OPEN sent to EDGE")

    # ==== MAIN DOOR (cửa AI, mở cưỡng bức giống như AI đúng mặt) ====
    elif method == "main_force":
        cmd = {"cmd": "OPEN_AI"}
        sub_client.publish("home/door/ai", json.dumps(cmd), qos=1)
        print(">>> Main door force OPEN sent to EDGE:", cmd)

    # ==== FAN: ON / OFF / AUTO ====
    elif method in ("fan_on", "fan_off", "fan_auto"):
        if method == "fan_on":
            mode = "on"
        elif method == "fan_off":
            mode = "off"
        else:
            mode = "auto"

        payload = {"mode": mode}
        sub_client.publish("home/fan/cmd", json.dumps(payload), qos=1)
        print(">>> Fan control sent to EDGE:", payload)

    # ==== GARDEN DOOR ====
    elif method == "garden_force":
        sub_client.publish("home/door/garden", "OPEN", qos=1)
        print(">>> Garden door OPEN sent to EDGE")

    # ==== POOL LED: ON / OFF / AUTO ====
    elif method in ("pool_led_on", "pool_led_off", "pool_led_auto"):
        if method == "pool_led_on":
            mode = "on"
        elif method == "pool_led_off":
            mode = "off"
        else:
            mode = "auto"

        payload = {"mode": mode}
        sub_client.publish("home/pool_led/cmd", json.dumps(payload), qos=1)
        print(">>> Pool LED control sent to EDGE:", payload)

    # ==== POOL PUMP CONTROL ====
    elif method == "pump":
        # Yêu cầu: nhấn nút là BẬT bơm, không cần tắt bằng RPC
        payload = {"pump": True}
        sub_client.publish("home/pump/cmd", json.dumps(payload), qos=1)
        print(">>> Pump control sent to EDGE:", payload)

def main():
    # ----- Core IoT client -----
    core_client.username_pw_set(CORE_TOKEN, "")
    core_client.on_connect = on_core_connect
    core_client.on_message = on_core_message
    core_client.connect(CORE_BROKER, CORE_PORT, 60)
    core_client.loop_start()   # chạy nền

    # ----- EDGE client -----
    sub_client.on_connect = on_sub_connect
    sub_client.on_message = on_sub_message
    sub_client.connect(EDGE_BROKER, EDGE_PORT, 60)

    print("Connecting to EDGE broker", EDGE_BROKER, "...")
    sub_client.loop_forever()


if __name__ == "__main__":
    main()
