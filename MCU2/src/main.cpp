#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

#include "globals.h"
#include "sensors.h"

// ================== WIFI + MQTT (EDGE BROKER) ==================
#define WIFI_SSID     "___YOUR WIFI NAME___"
#define WIFI_PASS     "___YOUR WIFI PASSWORD___"

#define MQTT_HOST     "___YOUR IPv4 ADRESS___FOR EXAMPLE___192.168.120.250"
#define MQTT_PORT   1883

// Telemetry từ node2 (Yolo Uno 2) -> Python bridge
#define MQTT_TOPIC_TELEMETRY  "home/nodes/node2"
#define GARDEN_CMD_TOPIC      "home/door/garden"
#define DOOR_AI_CMD_TOPIC     "home/door/ai"
#define GARAGE_CMD_TOPIC      "home/door/garage"
#define FAN_CMD_TOPIC         "home/fan/cmd"
#define POOL_LED_CMD_TOPIC    "home/pool_led/cmd"
// ================== AES KEY ==================
static const uint8_t AES_KEY[16] = {
  0x01, 0x02, 0x03, 0x04,
  0x05, 0x06, 0x07, 0x08,
  0x09, 0x0A, 0x0B, 0x0C,
  0x0D, 0x0E, 0x0F, 0x10
};

// ================== NEO-PIXEL ==================
void set_led(bool on) {
    glob_led_state = on;

    // màu cam (có thể chỉnh lại cho đậm / nhạt tùy ý)
    uint32_t orange = px.Color(255, 100, 0);

    if (on) {
        // bật cả 4 led
        for (int i = 0; i < NUM_PIXELS; ++i) {
            px.setPixelColor(i, orange);
        }
    } else {
        // tắt cả 4 led
        for (int i = 0; i < NUM_PIXELS; ++i) {
            px.setPixelColor(i, 0, 0, 0);
        }
    }

    px.show();
}


// ================== BIẾN TOÀN CỤC PHỤ TRỢ ==================
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Servo AI: các biến này bạn khai báo / định nghĩa trong globals.*
extern bool     glob_ai_door_open;
extern uint32_t glob_ai_door_deadline_ms;

// ================== HÀM MÃ HÓA AES-ECB + PKCS7 + BASE64 ==================
String aesEncryptToBase64(const String &plain) {
  const size_t blockSize = 16;
  size_t len       = plain.length();
  size_t paddedLen = ((len / blockSize) + 1) * blockSize;

  std::vector<unsigned char> input(paddedLen);
  memcpy(input.data(), plain.c_str(), len);
  uint8_t padVal = paddedLen - len;
  for (size_t i = len; i < paddedLen; ++i) {
    input[i] = padVal;
  }

  std::vector<unsigned char> output(paddedLen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

  for (size_t off = 0; off < paddedLen; off += blockSize) {
    mbedtls_aes_crypt_ecb(&aes,
                          MBEDTLS_AES_ENCRYPT,
                          input.data()  + off,
                          output.data() + off);
  }
  mbedtls_aes_free(&aes);

  // Base64
  size_t outLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outLen,
                        output.data(), paddedLen);

  std::vector<unsigned char> b64(outLen + 1);
  mbedtls_base64_encode(b64.data(), outLen, &outLen,
                        output.data(), paddedLen);
  b64[outLen] = '\0';

  return String((char*)b64.data());
}

// ================== WIFI ==================
void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("[NODE2] WiFi connecting to %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[NODE2] WiFi OK. IP: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[NODE2] WiFi FAIL!");
  }
}

// ================== MQTT ==================
void mqttCallback(char *topic, byte *payload, unsigned int length);

void mqttReconnect() {
  if (mqtt.connected()) return;

  Serial.print("[NODE2] MQTT connecting ... ");
  if (mqtt.connect("NODE2_CLIENT")) {
    Serial.println("OK");
    // Nhận lệnh 
    mqtt.subscribe(DOOR_AI_CMD_TOPIC);
    mqtt.subscribe(GARDEN_CMD_TOPIC);
    mqtt.subscribe(GARAGE_CMD_TOPIC);
    mqtt.subscribe(FAN_CMD_TOPIC);
    mqtt.subscribe(POOL_LED_CMD_TOPIC);
  } else {
    Serial.printf("fail rc=%d\n", mqtt.state());
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String t(topic);
  String body;
  for (unsigned int i = 0; i < length; ++i) {
    body += (char)payload[i];
  }

  Serial.printf("[NODE2 MQTT RX] topic=%s payload=%s\n",
                t.c_str(), body.c_str());

  if (t == DOOR_AI_CMD_TOPIC) {
    if (body.indexOf("OPEN_AI") >= 0) {
      glob_ai_door_open        = true;
      glob_ai_door_deadline_ms = millis() + 3000; // mở 3 giây
      Serial.println("[NODE2] AI door command -> OPEN_AI");
    }
  }

  if (t == GARDEN_CMD_TOPIC) {
    garden_servo_open_ms(3000);  // mở 3 giây
  }

  // ==== GARAGE DOOR FORCE ====
  if (t == GARAGE_CMD_TOPIC) {
    // hiện tại payload không dùng, chỉ cần nhận là mở
    door_force_open_ms(10000);  // mở gara 10s (tùy chỉnh)
  }

  // ==== FAN: ON / OFF / AUTO ====
   if (t == FAN_CMD_TOPIC) {
    if (body.indexOf("\"mode\": \"on\"") >= 0 || body.indexOf("\"mode\":\"on\"") >= 0) {
      fan_force_set(true, true);        // cưỡng bức bật
      Serial.println("[Fan RPC] FORCE ON");
    } else if (body.indexOf("\"mode\": \"off\"") >= 0 || body.indexOf("\"mode\":\"off\"") >= 0) {
      fan_force_set(true, false);       // cưỡng bức tắt
      Serial.println("[Fan RPC] FORCE OFF");
    } else if (body.indexOf("\"mode\": \"auto\"") >= 0 || body.indexOf("\"mode\":\"auto\"") >= 0) {
      fan_force_set(false, false);      // quay lại chế độ PIR
      Serial.println("[Fan RPC] AUTO (PIR)");
    }
  }

  // ==== POOL LED: ON / OFF / AUTO ====
     // ==== POOL LED: ON / OFF / AUTO ====
  if (t == POOL_LED_CMD_TOPIC) {
    if (body.indexOf("\"mode\": \"on\"") >= 0 || body.indexOf("\"mode\":\"on\"") >= 0) {
      glob_led_override = true;
      set_led(true);
      Serial.println("[POOL LED] FORCE ON");
    } else if (body.indexOf("\"mode\": \"off\"") >= 0 || body.indexOf("\"mode\":\"off\"") >= 0) {
      glob_led_override = true;
      set_led(false);
      Serial.println("[POOL LED] FORCE OFF");
    } else if (body.indexOf("\"mode\": \"auto\"") >= 0 || body.indexOf("\"mode\":\"auto\"") >= 0) {
      glob_led_override = false;  // giao lại cho light_task
      Serial.println("[POOL LED] AUTO (light sensor)");
    }
  }
}


// ================== TASK GỬI TELEMETRY (ENCRYPTED) ==================
void task_mqtt_publish(void *pv) {
  const TickType_t period = pdMS_TO_TICKS(2000);
  TickType_t last = xTaskGetTickCount();

  while (1) {
    wifiConnect();
    mqttReconnect();
    mqtt.loop();

    // JSON thuần (đảm bảo các biến này có trong globals.*)
    String plain = "{";
    plain += "\"temperature\":" + String(glob_temperature, 1) + ",";
    plain += "\"humidity\":"    + String(glob_humidity,    1) + ",";
    plain += "\"light_raw\":"   + String(glob_light_raw)       + ",";
    plain += "\"led_state\":"   + String(glob_led_state ? "true" : "false") + ",";
    plain += "\"distance_cm\":" + String(glob_distance, 2)     + ",";
    plain += "\"door_open\":"   + String(glob_door_open ? "true" : "false") + ",";
    plain += "\"fan_state\":"   + String(glob_fan_state ? "true" : "false");
    plain += "}";

    String cipherB64 = aesEncryptToBase64(plain);

    String payload = "{";
    payload += "\"dev\":\"node2\",";
    payload += "\"cipher\":\"" + cipherB64 + "\"";
    payload += "}";

    mqtt.publish(MQTT_TOPIC_TELEMETRY, payload.c_str());

    Serial.println("[NODE2 PLAIN]  " + plain);
    Serial.println("[NODE2 CIPHER] " + payload);

    vTaskDelayUntil(&last, period);
  }
}

// ================== SETUP / LOOP ==================
void setup() {
  Serial.begin(115200);
  delay(200);
  px.begin();
  px.setBrightness(20);
  set_led(false);


  // Các task sensor/điều khiển đã có sẵn trong sensors.cpp
  xTaskCreate(temp_humi_monitor, "DHT20",   4096, NULL, 2, NULL);
  xTaskCreate(light_task,        "LIGHT",   4096, NULL, 2, NULL);
  xTaskCreate(ultrasonic_task,   "ULTRA",   4096, NULL, 2, NULL);
  xTaskCreate(fan_control_task,  "FAN",     4096, NULL, 2, NULL);
  xTaskCreate(ai_servo_task,     "AI_SERVO",4096, NULL, 2, NULL);
  xTaskCreate(garden_servo_task, "GARDEN",  3072, NULL, 2, NULL);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  xTaskCreate(task_mqtt_publish, "MQTT",    4096, NULL, 2, NULL);
}

void loop() {}
