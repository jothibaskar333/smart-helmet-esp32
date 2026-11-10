#include "blind_spot.h"

#include <esp_timer.h>

#include "config.h"

struct EchoSensor {
  uint8_t trigPin;
  uint8_t echoPin;
  volatile int64_t riseUs;
  volatile int64_t widthUs;  // -1 while waiting
  volatile TaskHandle_t waiter;
  float history[BS_MEDIAN_WINDOW];
  uint8_t next;
};

static EchoSensor s_left{PIN_US_TRIG_L, PIN_US_ECHO_L, 0, -1, nullptr, {}, 0};
static EchoSensor s_right{PIN_US_TRIG_R, PIN_US_ECHO_R, 0, -1, nullptr, {}, 0};

static void IRAM_ATTR echoIsr(void* arg) {
  EchoSensor* s = static_cast<EchoSensor*>(arg);
  const int64_t now = esp_timer_get_time();
  if (gpio_get_level(static_cast<gpio_num_t>(s->echoPin))) {
    s->riseUs = now;
  } else if (s->riseUs) {
    s->widthUs = now - s->riseUs;
    s->riseUs = 0;
    BaseType_t woken = pdFALSE;
    if (s->waiter) vTaskNotifyGiveFromISR(s->waiter, &woken);
    if (woken) portYIELD_FROM_ISR();
  }
}

static void initSensor(EchoSensor& s) {
  pinMode(s.trigPin, OUTPUT);
  digitalWrite(s.trigPin, LOW);
  pinMode(s.echoPin, INPUT);
  for (float& h : s.history) h = BS_NO_ECHO_CM;
  attachInterruptArg(digitalPinToInterrupt(s.echoPin), echoIsr, &s, CHANGE);
}

void blindSpotInit() {
  initSensor(s_left);
  initSensor(s_right);
}

// One measurement in cm, or BS_NO_ECHO_CM on timeout.
static float ping(EchoSensor& s) {
  s.widthUs = -1;
  s.riseUs = 0;
  s.waiter = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0);  // clear any stale notification

  digitalWrite(s.trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(s.trigPin, HIGH);
  delayMicroseconds(10);  // 10 us trigger pulse, as on the slides
  digitalWrite(s.trigPin, LOW);

  ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BS_ECHO_TIMEOUT_MS));
  s.waiter = nullptr;

  const int64_t width = s.widthUs;
  if (width <= 0) return BS_NO_ECHO_CM;
  const float cm = width * 0.034f / 2.0f;  // same conversion as the slide code
  return cm > BS_NO_ECHO_CM ? BS_NO_ECHO_CM : cm;
}

static float median(const float* v) {
  float tmp[BS_MEDIAN_WINDOW];
  memcpy(tmp, v, sizeof(tmp));
  for (int i = 1; i < BS_MEDIAN_WINDOW; ++i) {  // insertion sort, N is tiny
    const float key = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      --j;
    }
    tmp[j + 1] = key;
  }
  return tmp[BS_MEDIAN_WINDOW / 2];
}

static float measure(EchoSensor& s) {
  float d = ping(s);
  if (d <= BS_MIN_CM) d = BS_NO_ECHO_CM;  // slide rule: ignore < 10 cm (0 = timeout on slides)
  s.history[s.next] = d;
  s.next = (s.next + 1) % BS_MEDIAN_WINDOW;
  return median(s.history);
}

BlindSpotReading blindSpotStep() {
  BlindSpotReading r;
  r.leftCm = measure(s_left);
  vTaskDelay(pdMS_TO_TICKS(BS_INTER_SENSOR_GAP_MS));
  r.rightCm = measure(s_right);
  r.alertLeft = r.leftCm > BS_MIN_CM && r.leftCm < BS_ALERT_CM;
  r.alertRight = r.rightCm > BS_MIN_CM && r.rightCm < BS_ALERT_CM;
  return r;
}
