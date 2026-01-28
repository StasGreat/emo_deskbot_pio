#pragma once
#include <Arduino.h>
#include "driver/i2s.h"

enum class SfxId : uint8_t {
  Ready = 0,
  ChirpUp,
  ChirpMid,
  Pop,
  Sigh,
  Scared,
  Down,
  Annoyed,
  Count
};

class AudioSfx {
public:
  void begin();
  void stop();
  void play(SfxId id, uint32_t nowMs);
  void setRateLimitMs(uint32_t ms) { rateLimitMs = ms; }

private:
  i2s_port_t port = I2S_NUM_0;
  uint32_t lastSfxAt = 0;
  uint32_t rateLimitMs = 700;

  void playBlocking(SfxId id);
};
