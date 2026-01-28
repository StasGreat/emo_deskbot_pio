#include "imu_bmi270.h"
#include "config.h"
#include <Wire.h>
#include <math.h>

#include <SparkFun_BMI270_Arduino_Library.h>

static BMI270 imu;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool ImuBmi270::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (imu.beginI2C(0x68) == BMI2_OK) { ok = true; return true; }
  if (imu.beginI2C(0x69) == BMI2_OK) { ok = true; return true; }

  ok = false;
  return false;
}

float ImuBmi270::computeShakeStrength(float amagG) {
  float s = (amagG - shakeThresholdG) / 1.0f; // +1g above threshold => 1.0
  return clampf(s, 0.0f, 1.0f);
}

float ImuBmi270::computeTiltDeg(float axG, float ayG, float azG) {
  float mag = sqrtf(axG*axG + ayG*ayG + azG*azG);
  if (mag < 0.2f) return 0.0f;
  float c = fabsf(azG) / mag;
  c = clampf(c, 0.0f, 1.0f);
  return acosf(c) * 57.2957795f; // rad->deg
}

void ImuBmi270::update(uint32_t nowMs, State currentState, EventQueue& q) {
  if (!ok) return;

  int8_t r = imu.getSensorData();
  if (r != BMI2_OK) return;

  // Library already converts to g's.
  float axRaw = imu.data.accelX;
  float ayRaw = imu.data.accelY;
  float azRaw = imu.data.accelZ;

  // Low-pass accel for orientation (helps jitter)
  ax = ax * (1.0f - accelLp) + axRaw * accelLp;
  ay = ay * (1.0f - accelLp) + ayRaw * accelLp;
  az = az * (1.0f - accelLp) + azRaw * accelLp;

  float axG = ax;
  float ayG = ay;
  float azG = az;

  // roll/pitch from gravity vector (deg)
  // roll: rotation around X axis (left/right tilt)
  // pitch: rotation around Y axis (forward/back tilt)
  roll = atan2f(ayG, azG) * 57.2957795f;
  pitch = atan2f(-axG, sqrtf(ayG*ayG + azG*azG)) * 57.2957795f;

  float amagG = sqrtf(axG*axG + ayG*ayG + azG*azG);

  // Shake detection (magnitude spike)
  if (amagG >= shakeThresholdG) {
    if (nowMs - lastShakeAt >= shakeMinIntervalMs) {
      lastShakeAt = nowMs;
      float strength = computeShakeStrength(amagG);
      q.push({EventType::Shake, strength, 0, nowMs});
    }
  }

  // Tilt detection with hysteresis
  float tiltDeg = computeTiltDeg(axG, ayG, azG);

  if (!tilted && tiltDeg >= tiltThresholdDeg) {
    if (nowMs - lastTiltAt >= tiltMinIntervalMs) {
      lastTiltAt = nowMs;
      tilted = true;
      q.push({EventType::Tilt, clampf(tiltDeg / 60.0f, 0.0f, 1.0f), 0, nowMs});
    }
  } else if (tilted && tiltDeg <= (tiltThresholdDeg * 0.6f)) {
    tilted = false;
  }

  (void)currentState;
}
