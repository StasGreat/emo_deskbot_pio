#pragma once
#include <Arduino.h>
#include "audio_i2s.h"

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
  void begin(AudioI2S& i2s);
  void stop();
  void play(SfxId id, uint32_t nowMs);
  void setRateLimitMs(uint32_t ms) { rateLimitMs = ms; }

private:
  AudioI2S* i2sRef = nullptr;
  uint32_t lastSfxAt = 0;
  uint32_t rateLimitMs = 700;

  void playBlocking(SfxId id);
};
