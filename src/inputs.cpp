#include "inputs.h"
#include "config.h"

void Inputs::begin() {
  // PTT input
  #if PTT_ACTIVE_LOW
    pinMode(PIN_PTT, INPUT_PULLUP);
  #else
    pinMode(PIN_PTT, INPUT);
  #endif

  // Touch input
  pinMode(PIN_TOUCH, INPUT);
}

bool Inputs::readPttActive() {
  int v = digitalRead(PIN_PTT);
  #if PTT_ACTIVE_LOW
    return v == LOW;
  #else
    return v == HIGH;
  #endif
}

bool Inputs::readTouchActive() {
  int v = digitalRead(PIN_TOUCH);
  #if TOUCH_ACTIVE_HIGH
    return v == HIGH;
  #else
    return v == LOW;
  #endif
}

void Inputs::sample(uint32_t nowMs, EventQueue& q) {
  bool touch = readTouchActive();
  bool ptt = readPttActive();

  // Touch edges
  if (touch && !lastTouch) {
    lastTouch = true;
    touchDownAt = nowMs;
    touchLongFired = false;
    q.push({EventType::TouchDown, 1.0f, 0, nowMs});
  }
  if (!touch && lastTouch) {
    lastTouch = false;
    uint32_t dur = nowMs - touchDownAt;
    q.push({EventType::TouchUp, 1.0f, dur, nowMs});
  }
  // Touch long
  if (touch && !touchLongFired && (nowMs - touchDownAt >= 2000)) {
    touchLongFired = true;
    q.push({EventType::TouchLong, 1.0f, nowMs - touchDownAt, nowMs});
  }

  // PTT edges
  if (ptt && !lastPtt) {
    lastPtt = true;
    q.push({EventType::PttDown, 1.0f, 0, nowMs});
  }
  if (!ptt && lastPtt) {
    lastPtt = false;
    q.push({EventType::PttUp, 1.0f, 0, nowMs});
  }

  // 1s tick
  if (nowMs - lastTick1s >= 1000) {
    lastTick1s = nowMs;
    q.push({EventType::Tick1s, 0, 0, nowMs});
  }

  // Mock sensors (replace with real BMI270/INMP441 detectors later)
  if (mockSound > 0.75f) q.push({EventType::SoundPeak, mockSound, 0, nowMs});
  if (mockShake > 0.60f) q.push({EventType::Shake, mockShake, 0, nowMs});
  if (mockTilt) {
    q.push({EventType::Tilt, 0.4f, 0, nowMs});
    mockTilt = false;
  }
}
