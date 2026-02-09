#include "globals.h"
float glob_temperature = 25.0f;
float glob_humidity = 55.0f;
volatile bool pump_on = false;
volatile int moist_value = 0;
volatile int  gas_value  = 0;
volatile bool gas_alert  = false;