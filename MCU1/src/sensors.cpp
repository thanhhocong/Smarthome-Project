#include "sensors.h"
void pump_control(void *pvParameters) {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); 
  while (1) {
    int raw = analogRead(MOIST_SENSOR_PIN);
    moist_value = raw;   

    Serial.printf("Độ ẩm đất raw = %d, pump_on=%d\n", raw, pump_on ? 1 : 0);
    if (raw >= 2400) {
      if (pump_on) {
        Serial.println("[PUMP] Moisture high -> auto stop & latch OFF");
      }
      pump_on = false;          
      analogWrite(PUMP_PIN, 0);
    } else {
      if (pump_on) {
        analogWrite(PUMP_PIN, 250);   
      } else {
        analogWrite(PUMP_PIN, 0);    
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void gas_monitor(void *pvParameters) {
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);  // tắt quạt ban đầu

    while (1) {
        int raw = analogRead(GAS_SENSOR_PIN);
        gas_value = raw;

        if (raw > GAS_THRESHOLD) {
            gas_alert = true;
            digitalWrite(FAN_PIN, HIGH);   // bật quạt
        } else {
            gas_alert = false;
            digitalWrite(FAN_PIN, LOW);    // tắt quạt
        }

        Serial.printf("MQ135 raw=%d alert=%d\n", raw, gas_alert ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

