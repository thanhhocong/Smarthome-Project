#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <PubSubClient.h>

#include "globals.h"
#include "sensors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

// ================== WIFI + MQTT CONFIG ==================
#define WIFI_SSID     "___YOUR WIFI NAME___"
#define WIFI_PASS     "___YOUR WIFI PASSWORD___"

#define MQTT_HOST     "___YOUR IPv4 ADRESS___FOR EXAMPLE___192.168.120.250"
#define MQTT_PORT     1883
#define MQTT_TOPIC    "home/nodes/node1"
#define PUMP_CMD_TOPIC "home/pump/cmd"

// ================== AES KEY ==================
static const uint8_t AES_KEY[16] = {
  0x01,0x02,0x03,0x04,
  0x05,0x06,0x07,0x08,
  0x09,0x0A,0x0B,0x0C,
  0x0D,0x0E,0x0F,0x10
};

// ====== MQTT CLIENT ======
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ================== AES Encrypt → Base64 ==================
String aesEncryptToBase64(const String &plain) {
  size_t len = plain.length();
  size_t blockSize = 16;
  size_t paddedLen = ((len / blockSize) + 1) * blockSize;

  std::vector<unsigned char> input(paddedLen);
  memcpy(input.data(), plain.c_str(), len);
  uint8_t pad = paddedLen - len;
  for (size_t i = len; i < paddedLen; i++) input[i] = pad;

  std::vector<unsigned char> output(paddedLen);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

  for (size_t i = 0; i < paddedLen; i += blockSize) {
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                          input.data() + i,
                          output.data() + i);
  }
  mbedtls_aes_free(&aes);

  // Base64
  size_t outLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outLen, output.data(), paddedLen);

  std::vector<unsigned char> b64(outLen + 1);
  mbedtls_base64_encode(b64.data(), outLen, &outLen, output.data(), paddedLen);
  b64[outLen] = '\0';

  return String((char*)b64.data());
}

// ================== WIFI ==================
void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("WiFi connecting to %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK. IP: %s\n",
        WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi FAIL!");
  }
}

// ================== MQTT ==================
void mqttReconnect() {
  if (mqtt.connected()) return;

  Serial.print("MQTT connecting ... ");
  if (mqtt.connect("NODE1_CLIENT")) {
    Serial.println("OK");
    mqtt.subscribe(PUMP_CMD_TOPIC);
  } else {
    Serial.printf("fail rc=%d\n", mqtt.state());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String body;
  for (unsigned int i = 0; i < length; ++i) {
    body += (char)payload[i];
  }

  Serial.printf("[NODE1 MQTT RX] topic=%s payload=%s\n",
                t.c_str(), body.c_str());

  if (t == PUMP_CMD_TOPIC) {
    // Kỳ vọng JSON: {"pump": true} hoặc {"pump": false}
    bool on = false;
    if (body.indexOf("true") >= 0) {
      on = true;
    }
    pump_on = on;
    Serial.printf("[PUMP] RPC set pump_on = %d\n", pump_on ? 1 : 0);
  }
}


// ================== TASK PUBLISH DỮ LIỆU ==================
void task_mqtt_publish(void *pv) {
  const TickType_t period = pdMS_TO_TICKS(2000);
  TickType_t last = xTaskGetTickCount();

  while (1) {
    wifiConnect();
    mqttReconnect();
    mqtt.loop();

    // ======================= JSON THUẦN =======================
    String plain = "{";
    plain += "\"moist\":"      + String(moist_value) + ",";
    plain += "\"pump_on\":"    + String(pump_on ? "true" : "false") + ",";
    plain += "\"gas_raw\":"    + String(gas_value) + ",";
    plain += "\"gas_alert\":"  + String(gas_alert ? "true" : "false");
    plain += "}";

    // ======================= ENCRYPT ==========================
    String cipher = aesEncryptToBase64(plain);

    // ======================= ĐÓNG GÓI ==========================
    String payload = "{";
    payload += "\"dev\":\"node1\",";
    payload += "\"cipher\":\"" + cipher + "\"";
    payload += "}";

    mqtt.publish(MQTT_TOPIC, payload.c_str());

    Serial.println("=== NODE1 ===");
    Serial.println(plain);
    Serial.println(payload);

    vTaskDelayUntil(&last, period);
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(200);

  // Sensors
  xTaskCreate(pump_control, "PUMP", 4096, NULL, 2, NULL);

  // Gas sensor
  xTaskCreate(gas_monitor, "GAS", 4096, NULL, 2, NULL);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  xTaskCreate(task_mqtt_publish, "MQTT", 4096, NULL, 2, NULL);
}

void loop() {
mqtt.loop();  
delay(5);
}
