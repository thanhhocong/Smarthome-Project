#ifndef __SENSORS__
#define __SENSORS__
#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"
#include "globals.h"
#include <Wire.h>
#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#define NEOPIXEL_PIN 6
#define NUM_PIXELS   4
extern Adafruit_NeoPixel px;
// I2C cho DHT20 + LCD
#define SDA_PIN   11
#define SCL_PIN   12

// Ultrasonic Sensor Pins (TRIG, ECHO)
#define ULTRASONIC_TRIG_PIN 8
#define ULTRASONIC_ECHO_PIN 9

// ==== PIR + Quạt (Relay) ====
#define PIR_SENSOR_PIN   10   
#define FAN_RELAY_PIN    18  


// --- Servo (điều khiển khóa) ---
#define SERVO_PIN            5   
#define SERVO_AI_PIN         48
#define SERVO_GARDEN_PIN     47   
#define DOOR_OPEN_ANGLE      90
#define DOOR_CLOSED_ANGLE     0
#define NEAR_CM              10.0f     // ngưỡng mở
#define HYSTERESIS_CM        1.0f     // chống giật

static uint8_t s_pin_adc = 1;
static int     s_threshold = 1000;

static Servo   s_servo;          // servo cửa chính (ultrasonic)
static bool    s_servo_open = false;
static uint32_t s_servo_until_ms = 0;  // thời điểm đóng sau khi mở

// Servo Garden
static Servo   s_servo_garden;
static bool    s_garden_open = false;
static uint32_t s_garden_until_ms = 0;

static inline void servo_set_angle(int deg) {
  deg = constrain(deg, 0, 180);
  s_servo.write(deg);
}
static inline void garden_set_angle(int deg) {
  deg = constrain(deg, 0, 180);
  s_servo_garden.write(deg);
}
static Servo ai_servo; 

//dht20
void temp_humi_monitor(void *pvParameters);
//light
void light_init(uint8_t pin_adc, uint8_t pin_led, int threshold);
void light_task(void* pv);
//sonic
void ultrasonic_task(void *pvParameters); 
//fan+pir
void fan_force_set(bool enable, bool on);
void fan_control_task(void *pvParameters);
void ai_servo_task(void *pv);
void set_led(bool on);

void garden_servo_task(void *pvParameters);
void garden_servo_open_ms(uint32_t duration_ms);
void door_force_open_ms(uint32_t duration_ms);
#endif