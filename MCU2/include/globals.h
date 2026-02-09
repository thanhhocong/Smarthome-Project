#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
//dht20
extern float glob_temperature;
extern float glob_humidity;
//light+led
extern int   glob_light_raw;     
extern bool  glob_led_state;     
extern bool  glob_led_override;  
//servo+ultrasonic
extern float glob_distance;
extern bool  glob_door_open;
//fan
extern bool glob_fan_state;
//AI
extern bool     glob_ai_door_open;          
extern uint32_t glob_ai_door_deadline_ms;  

#endif