#pragma once
#include <Arduino.h>
#include "types.h"
#include "event_queue.h"

// BMI270 motion detector (I2C) using SparkFun_BMI270_Arduino_Library.
// Emits events:
// - EventType::Shake (strength 0..1)
// - EventType::Tilt  (strength 0..1)
//
// The module uses Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL).

class ImuBmi270 {
public:
  bool begin(); // true if initialized

  void update(uint32_t nowMs, State currentState, EventQueue& q);

  // Tuning
  void setShakeThresholdG(float g) { shakeThresholdG = g; }        // e.g., 1.6
  void setShakeMinIntervalMs(uint32_t ms) { shakeMinIntervalMs = ms; }
  void setTiltThresholdDeg(float deg) { tiltThresholdDeg = deg; }  // e.g., 25
  void setTiltMinIntervalMs(uint32_t ms) { tiltMinIntervalMs = ms; }
  void setAccelLowpass(float a) { accelLp = a; }

  bool isOk() const { return ok; }

  // Orientation (degrees). Updated on each update() call.
  float rollDeg() const { return roll; }
  float pitchDeg() const { return pitch; }
  // Smoothed acceleration in g (approx gravity vector in static conditions)
  float axG() const { return ax; }
  float ayG() const { return ay; }
  float azG() const { return az; }

private:
  bool ok = false;

  float shakeThresholdG = 1.6f;
  uint32_t shakeMinIntervalMs = 500;
  uint32_t lastShakeAt = 0;

  float tiltThresholdDeg = 25.0f;
  uint32_t tiltMinIntervalMs = 800;
  uint32_t lastTiltAt = 0;
  bool tilted = false;

  // smoothed accel and derived angles
  float ax = 0.0f, ay = 0.0f, az = 1.0f;
  float roll = 0.0f, pitch = 0.0f;
  float accelLp = 0.15f; // low-pass factor (0..1)

  float computeShakeStrength(float amagG);
  float computeTiltDeg(float axG, float ayG, float azG);
};
