#pragma once
#include <Arduino.h>
#include "audio_i2s.h"

enum class Wave : uint8_t { Sine = 0, Square = 1, Noise = 2 };

struct SfxDef {
  float f0;
  float f1;
  uint16_t ms;
  float amp;   // 0..1
  Wave wave;
};

enum class SfxId : uint8_t {
  Ready = 0,
  ChirpUp,
  ChirpMid,
  Pop,
  Sigh,
  Scared,
  Down,
  Annoyed,
  Think,
  Confirm,
  Soft,
  Wake,
  Count
};

class AudioSfx {
public:
  void begin(AudioI2S& i2s);
  void stop();
  void play(SfxId id, uint32_t nowMs);
  // priority: 0 = low (idle), 1 = normal, 2 = high (scared/surprised)
  void playPriority(SfxId id, uint8_t priority, uint32_t nowMs);

  // Runtime tuning
  SfxDef getDef(SfxId id) const;
  void setDef(SfxId id, const SfxDef& def);
  void resetDefaults();
  void dumpDefs(Stream& out) const;
  void setRateLimitMs(uint32_t ms) { rateLimitMs = ms; }

private:
  AudioI2S* i2sRef = nullptr;
  uint32_t lastSfxAt = 0;
  uint32_t rateLimitMs = 700;
  uint8_t lastPriority = 1;

  void playBlocking(SfxId id);
};
