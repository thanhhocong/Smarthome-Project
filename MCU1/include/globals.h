#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern float glob_temperature;
extern float glob_humidity;
extern volatile bool pump_on;
extern volatile int moist_value;
extern volatile int gas_value;    // giá trị ADC từ MQ-135
extern volatile bool gas_alert;   // true nếu phát hiện khói/gas vượt ngưỡng
#endif