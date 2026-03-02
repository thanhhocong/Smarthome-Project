# Smart Home — IoT Logic Design Project

## Overview

This project implements a **secure, hybrid IoT Smart Home system** that integrates local edge computing with cloud-based management. Two ESP32 microcontrollers act as sensor/actuator nodes, communicating over an encrypted MQTT network. A Python gateway bridges the local network to the **Core IoT** cloud platform for real-time monitoring and remote control.

Key goals:
- **Edge Security**: AES-128 encryption on the MCU before any data leaves the device.
- **Protocol Interoperability**: Python middleware translating between local MQTT and the cloud's JSON-RPC format.
- **AI Integration**: Local face recognition for contactless door entry with no cloud dependency.


## System Architecture

The system is organized into three layers:

| Layer | Components | Role |
|---|---|---|
| Perception | MCU 1, MCU 2 (ESP32) | Sensor data collection, actuator control |
| Network/Gateway | MQTT Broker, Python Bridge | Decryption, protocol translation, RPC routing |
| Application | Core IoT Dashboard | Visualization, remote control |

Data flow:
1. MCU 1 and MCU 2 collect sensor data, serialize it to JSON, and encrypt it with AES-128-ECB.
2. The encrypted payload is published to the local MQTT broker (`home/nodes/node1`, `home/nodes/node2`).
3. The Python gateway subscribes to `home/nodes/#`, decrypts the payload, and forwards clean JSON to Core IoT via `v1/devices/me/telemetry`.
4. User commands from the dashboard travel in reverse as RPC calls, which the Python bridge routes to the appropriate local MQTT topic.


## Features

### MCU 1 — Environmental Safety Node
- **Gas Monitoring**: MQ-135 sensor detects NH3, Benzene, Smoke and auto-triggers the exhaust fan via relay.
- **Smart Irrigation**: Resistive soil moisture sensor controls the water pump via PWM with dry-run protection.
- **Real-time Multitasking**: FreeRTOS tasks ensure gas safety checks are never blocked by slower pump logic.

### MCU 2 — Smart Access & Home Automation
- **Ultrasonic Garage Door**: HC-SR04 detects an approaching vehicle and auto-opens the door for 10 seconds.
- **AI-Driven Main Door**: Face recognition unlocks the door; brute-force lockout triggers after 10 failed attempts.
- **Garden Gate**: Remote-triggered via the cloud dashboard.
- **Smart Pool Lighting**: LDR-based automatic NeoPixel (WS2812B) control with manual override.
- **Environmental Display**: DHT20 temperature/humidity shown on a 16x2 I2C LCD.
- **Motion-Activated Cooling**: PIR sensor triggers the fan with remote override capability.

### Security
- **AES-128-ECB Encryption** on all MQTT payloads using mbedtls on the ESP32.
- **PKCS#7 Padding + Base64 Encoding** for safe text transport over MQTT.
- **Python Decryption Gateway** restores plaintext before cloud forwarding.


## Hardware Components

| # | Component | Function |
|---|---|---|
| 1 | ESP32 (x2) | Main microcontrollers (MCU 1 & MCU 2) |
| 2 | MQ-135 | Gas / air quality sensor |
| 3 | Resistive Soil Moisture Sensor | Soil water content |
| 4 | DHT20 | Temperature & humidity (I2C) |
| 5 | HC-SR04 | Ultrasonic distance (garage door) |
| 6 | PIR HC-SR501 | Motion detection |
| 7 | LDR (Photoresistor) | Ambient light sensing |
| 8 | WS2812B NeoPixel Strip | Addressable RGB pool lighting |
| 9 | Servo Motors (x3) | Main door, garage door, garden gate |
| 10 | Relay Module (x2) | Fan & pump power switching |
| 11 | DC Water Pump (12V) | Irrigation |
| 12 | DC Exhaust Fan | Ventilation |
| 13 | 16x2 I2C LCD (PCF8574) | Environmental display |
| 14 | Webcam | Face recognition input |


## Software Stack

| Component | Technology |
|---|---|
| MCU Firmware | C++ (Arduino/ESP32 framework), FreeRTOS, mbedtls |
| MQTT Broker | Eclipse Mosquitto 2.x |
| Gateway / Bridge | Python 3, paho-mqtt, PyCryptodome |
| AI / Face Recognition | Python 3, OpenCV, scikit-learn (Isolation Forest) |
| Cloud Platform | Core IoT (ThingsBoard-based), app.coreiot.io |


## Dashboard & Remote Control

The **Core IoT** dashboard provides real-time monitoring and one-click actuator control.

**Monitored Telemetry:**

| Widget | Source | Description |
|---|---|---|
| Temperature | DHT20 (MCU 2) | Current room temperature (°C) |
| Humidity | DHT20 (MCU 2) | Relative humidity (%) |
| Light Intensity | LDR ADC (MCU 2) | Raw ADC value driving pool LED logic |
| Gas in Air | MQ-135 (MCU 1) | Raw ADC value; high = fan activates |
| Car At Door | HC-SR04 (MCU 2) | Distance in cm at garage entrance |
| Pool Water Pump | MCU 1 | Binary pump state (True/False) |

**RPC Control Buttons:**

| Button | RPC Method | Effect |
|---|---|---|
| Garage Door | `garage_force` | Force open garage door (10s) |
| Main Door | `main_force` | Simulate face ID unlock |
| Garden Door | `garden_force` | Open garden gate (3s) |
| Fan ON / OFF / AUTO | `fan_on/off/auto` | Control cooling fan mode |
| Pool LED ON/OFF/AUTO | `pool_led_on/off/auto` | Control NeoPixel strip |
| Pool PUMP WATER | `pump` | Toggle irrigation pump |
| Open Camera | `CamOn` | Activate face recognition camera |


## AI Face Recognition

The contactless entry system uses a two-stage pipeline.

**Detection** — Haar Cascade Classifier (Viola-Jones) locates the face in each frame in real time on CPU without requiring a GPU.

**Recognition** — Isolation Forest (one-class anomaly detection):
- Trained only on the owner's face vectors (64x64 grayscale, flattened to a 4096-feature vector).
- Outputs `1` for Owner (unlock door) or `-1` for Stranger (reject).
- Security lockout: camera auto-shuts after 10 consecutive failed attempts.

The inference engine (AI.py) operates as a state machine controlled via MQTT: it waits in an idle state until the `ai/camera_control` topic activates it, then begins checking faces at 1-second intervals.


## Security

### Encryption Pipeline (Edge to Cloud)

JSON data is serialized on the MCU, padded with PKCS#7, encrypted with AES-128-ECB using mbedtls, Base64-encoded, and published to the MQTT broker. The broker only ever sees the encrypted ciphertext.

### Decryption Pipeline (Python Gateway)

The Python gateway receives the Base64 string, decodes it, decrypts it with the shared AES key using PyCryptodome, strips the PKCS#7 padding, and forwards the recovered JSON to Core IoT.

**Shared Key** (must match on both MCU firmware and Python gateway):
```
0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08
0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F 0x10
```

> **Security Note**: The current implementation uses AES-ECB mode with a hardcoded key. For production use, upgrade to AES-GCM with TLS/SSL and dynamic key provisioning.


## Limitations & Future Work

**Current Limitations:**
- AES-ECB mode is vulnerable to pattern analysis on identical plaintext blocks.
- The symmetric key is hardcoded in firmware, which poses a risk if the firmware is extracted.
- Face recognition is sensitive to significant lighting changes and shadows.
- Network credentials require recompiling the firmware to change.

**Planned Improvements:**
1. **Dynamic Provisioning** — Bluetooth/Smart Config for Wi-Fi and MQTT credentials via a mobile app.
2. **Enhanced Cryptography** — Upgrade to AES-GCM and add TLS/SSL on MQTT connections.
3. **AI Optimization** — Replace Isolation Forest with a lightweight CNN; add NPU support for variable lighting conditions.
4. **OTA Updates** — Over-the-air firmware updates for MCU 1 and MCU 2.


## Full Demo at [Video](https://drive.google.com/file/d/1uezzFwOepZHmiUW3FAfqQpF2r_dIaLtJ/view?usp=sharing)
