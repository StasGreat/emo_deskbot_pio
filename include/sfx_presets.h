#pragma once
#include "audio_sfx.h"

// Named sound personalities (presets) for quick character tuning.
// Each preset provides a full table of SfxDef for all SfxId values.
// Apply by: applyPreset(PresetId::Cute, audio);

enum class PresetId : uint8_t {
  Default = 0,
  Cute,
  Serious,
  EmoLike,
  StackChanLike,
  Count
};

const char* presetName(PresetId id);
bool parsePreset(const char* s, PresetId* out);

// Apply preset into audio (overwrites current SFX table).
void applyPreset(PresetId id, AudioSfx& audio);
