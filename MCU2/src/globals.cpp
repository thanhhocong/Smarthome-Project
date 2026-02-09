#include "globals.h"
//dht20
float glob_temperature = 0.0f;
float glob_humidity = 0.0f;
//light
int   glob_light_raw   = 0;
bool  glob_led_state   = false;
bool  glob_led_override= false;
//ultrasonic
float glob_distance = 0.0f;
bool  glob_door_open = false;
//fan
bool glob_fan_state = false;
//AI
bool     glob_ai_door_open = false;
uint32_t glob_ai_door_deadline_ms = 0;

