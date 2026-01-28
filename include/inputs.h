#pragma once
#include "event_queue.h"

class Inputs {
public:
  void begin();
  void sample(uint32_t nowMs, EventQueue& q);

  // Debug toggles (if you want to inject events without sensors)
  void setMockSound(float v) { mockSound = v; }
  void setMockShake(float v) { mockShake = v; }
  void setMockTilt(bool v) { mockTilt = v; }
  bool touchActive() const { return lastTouch; }
  bool pttActive() const { return lastPtt; }

private:
  bool lastTouch = false;
  bool lastPtt = false;
  uint32_t touchDownAt = 0;
  bool touchLongFired = false;
  uint32_t lastTick1s = 0;

  float mockSound = 0.0f;
  float mockShake = 0.0f;
  bool mockTilt = false;

  bool readPttActive();
  bool readTouchActive();
};
