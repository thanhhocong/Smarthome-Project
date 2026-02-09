#include "sensors.h"
// ===== DHT20 + LCD =====
DHT20 dht20;
LiquidCrystal_I2C lcd(0x21, 16, 2);
Adafruit_NeoPixel px(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void temp_humi_monitor(void *pvParameters) {
  Wire.begin(SDA_PIN, SCL_PIN);
  dht20.begin();
  lcd.init();
  lcd.backlight();

  char lineBuf[32];

  while(1) {
    dht20.read();
    float temperature = dht20.getTemperature();
    float humidity    = dht20.getHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      temperature = humidity = -1;
    }

    glob_temperature = temperature;
    glob_humidity    = humidity;

    Serial.printf("Humidity: %.1f%%  Temp: %.1f C\n", humidity, temperature);

    lcd.clear();
    lcd.setCursor(0, 0);
    snprintf(lineBuf, sizeof(lineBuf), "Temp: %4.1f C", temperature);
    lcd.print(lineBuf);
    lcd.setCursor(0, 1);
    snprintf(lineBuf, sizeof(lineBuf), "Humid: %4.1f %%", humidity);
    lcd.print(lineBuf);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}


                     // lightttttttttttttttttttttttt

void light_init(uint8_t pin_adc, uint8_t /*pin_led*/, int threshold) {
  s_pin_adc   = pin_adc;
  s_threshold = threshold;
  pinMode(s_pin_adc, INPUT);
}

void light_task(void* /*pv*/) {
  const TickType_t period = pdMS_TO_TICKS(300);
  TickType_t last = xTaskGetTickCount();

  while(1) {
    int raw = analogRead(s_pin_adc);  
    glob_light_raw = raw;
    if (!glob_led_override) {
  bool on = (raw < s_threshold);   

  if (on != glob_led_state) {      
    glob_led_state = on;
    extern void set_led(bool on);
    set_led(on);
  }
}


    vTaskDelayUntil(&last, period);
  }
}
                       // ===== Ultrasonic Task =====
void ultrasonic_task(void*) {
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT_PULLDOWN);

  // servo init
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  s_servo.setPeriodHertz(50);
  s_servo.attach(SERVO_PIN, 500, 2400);
  servo_set_angle(DOOR_CLOSED_ANGLE);
  s_servo_open   = false;
  glob_door_open = false;
  s_servo_until_ms = 0;

  for (;;) {
    // đo khoảng cách
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000); // 30ms timeout
    float distance = (duration == 0) ? -1.0f : (duration * 0.0343f / 2.0f);
    glob_distance = distance;

    uint32_t now = millis();

    // Nếu đang đóng mà có người lại gần => mở 90° và hẹn giờ 10s
    if (!s_servo_open && distance > 0 && distance < NEAR_CM) {
      servo_set_angle(DOOR_OPEN_ANGLE);     // 90°
      s_servo_open    = true;
      glob_door_open  = true;
      s_servo_until_ms = now + 10000UL;     // 10 giây
      Serial.println("[Door] OPEN (ultrasonic, 10s)");
    }

    // Nếu đang mở mà đã hết 10s => đóng lại
    if (s_servo_open && now > s_servo_until_ms) {
      servo_set_angle(DOOR_CLOSED_ANGLE);   // 0°
      s_servo_open    = false;
      glob_door_open  = false;
      s_servo_until_ms = 0;
      Serial.println("[Door] CLOSE (timeout 10s)");
    }

    vTaskDelay(pdMS_TO_TICKS(150));  // vẫn đo mỗi 150ms để cập nhật glob_distance
  }
}
                      // ======== SERVO GARDEN ========
void garden_servo_open_ms(uint32_t duration_ms) {
  s_garden_open    = true;
  s_garden_until_ms = millis() + duration_ms;
  garden_set_angle(DOOR_OPEN_ANGLE);   // 90°
  Serial.printf("[Garden] OPEN for %lu ms\n", (unsigned long)duration_ms);
}

void garden_servo_task(void *pvParameters) {
  (void)pvParameters;
  // init servo Garden
  ESP32PWM::allocateTimer(4);
  s_servo_garden.setPeriodHertz(50);
  s_servo_garden.attach(SERVO_GARDEN_PIN, 500, 2400);
  garden_set_angle(DOOR_CLOSED_ANGLE);   // 0°
  s_garden_open    = false;
  s_garden_until_ms = 0;

  for (;;) {
    if (s_garden_open) {
      uint32_t now = millis();
      if (now > s_garden_until_ms) {
        // tự đóng sau khi hết thời gian
        garden_set_angle(DOOR_CLOSED_ANGLE);
        s_garden_open    = false;
        s_garden_until_ms = 0;
        Serial.println("[Garden] CLOSE");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // kiểm tra ~20 lần/giây
  }
}

void door_force_open_ms(uint32_t duration_ms) {
  uint32_t now = millis();
  servo_set_angle(DOOR_OPEN_ANGLE);   // 90°
  s_servo_open    = true;
  glob_door_open  = true;
  s_servo_until_ms = now + duration_ms;
  Serial.printf("[Door] FORCE OPEN for %lu ms\n",
                (unsigned long)duration_ms);
}
                       //===========PIR task===============
#define RELAY_ACTIVE_LOW 0

static bool s_fan_forced = false;  
static bool s_fan_target = false;   

static inline void fan_hw_write(bool on) {
  digitalWrite(FAN_RELAY_PIN,
    RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

static inline void fan_set_state(bool on) {
  glob_fan_state = on;
  fan_hw_write(on);
}

void fan_force_set(bool enable, bool on) {
  s_fan_forced = enable;
  s_fan_target = on;
  if (enable) fan_set_state(on);
}

void fan_control_task(void*) {
  pinMode(PIR_SENSOR_PIN, INPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  fan_set_state(false);

  const TickType_t period = pdMS_TO_TICKS(200); // quét mỗi 200ms
  TickType_t last = xTaskGetTickCount();

  while(1) {
    if (s_fan_forced) {
      // Nếu đang bị cưỡng chế (từ RPC) thì chỉ chạy theo lệnh
      fan_set_state(s_fan_target);
    } else {
      int pir = digitalRead(PIR_SENSOR_PIN);
      bool should_on = (pir == HIGH);

      if (should_on != glob_fan_state) {
        fan_set_state(should_on);
        Serial.printf("[Fan] %s\n", should_on ? "ON - Motion detected" : "OFF - No motion");
      }
    }

    vTaskDelayUntil(&last, period);
  }
}

void ai_servo_task(void *pv) {
    ai_servo.attach(SERVO_AI_PIN, 500, 2400);
    ai_servo.write(0); // đóng cửa ban đầu

    while(1) {
        // tự đóng nếu quá thời gian
        if (glob_ai_door_open && glob_ai_door_deadline_ms && millis() > glob_ai_door_deadline_ms) {
            glob_ai_door_open = false;
            ai_servo.write(0);     // góc đóng
            glob_ai_door_deadline_ms = 0;
            Serial.println("[AI Servo] Auto close");
        }

        if (glob_ai_door_open) {
            ai_servo.write(90);    // góc mở
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}