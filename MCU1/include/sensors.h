#ifndef __SENSORS__
#define __SENSORS__
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"
#include <Adafruit_NeoPixel.h>
#include "globals.h"

#define MOIST_SENSOR_PIN  2   // tuỳ chân analog bạn nối cảm biến ẩm đất
#define PUMP_PIN          6    // chân điều khiển relay bơm nước
#define GAS_SENSOR_PIN    1     // MQ135 tại A0
#define FAN_PIN           8     // Relay quạt tại D5
#define GAS_THRESHOLD     1200  // tuỳ chỉnh theo thực tế

void gas_monitor(void *pvParameters);
void temp_humi_monitor(void *pvParameters);
void pump_control(void *pvParameters);
#endif