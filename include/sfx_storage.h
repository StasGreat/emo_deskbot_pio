#pragma once
#include <Arduino.h>
#include "audio_sfx.h"
#include "sfx_presets.h"

// Persistent storage for SFX definitions (Preferences/NVS).
// Stores full SfxDef table + rateLimitMs + selected preset label.
//
// Namespace: "emo_sfx"
// Keys:
// - "ver" (uint32)
// - "preset" (string)
// - "rate" (uint32)
// - "defs" (bytes)

class SfxStorage {
public:
  bool load(AudioSfx& audio, PresetId* presetOut = nullptr);
  bool save(const AudioSfx& audio, PresetId preset);
  void clear();

  static constexpr uint32_t kVersion = 1;
};
