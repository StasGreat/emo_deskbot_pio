#include "sfx_presets.h"
#include <string.h>
#include <ctype.h>

static bool ieq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = (char)tolower((unsigned char)*a++);
    char cb = (char)tolower((unsigned char)*b++);
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

const char* presetName(PresetId id) {
  switch (id) {
    case PresetId::Default: return "default";
    case PresetId::Cute: return "cute";
    case PresetId::Serious: return "serious";
    case PresetId::EmoLike: return "emo";
    case PresetId::StackChanLike: return "stackchan";
    default: return "unknown";
  }
}

bool parsePreset(const char* s, PresetId* out) {
  if (!s || !out) return false;
  if (ieq(s, "default")) { *out = PresetId::Default; return true; }
  if (ieq(s, "cute")) { *out = PresetId::Cute; return true; }
  if (ieq(s, "serious")) { *out = PresetId::Serious; return true; }
  if (ieq(s, "emo") || ieq(s, "emo-like") || ieq(s, "emolike")) { *out = PresetId::EmoLike; return true; }
  if (ieq(s, "stackchan") || ieq(s, "stackchan-like") || ieq(s, "stackchanlike")) { *out = PresetId::StackChanLike; return true; }
  return false;
}

// Presets: designed for tiny speaker on MAX98357A. You can still tweak via Serial.
// Notes:
// - Cute: higher pitch, shorter, softer attack (sine mostly)
// - Serious: lower pitch, less "chirpy", more subdued
// - EmoLike: expressive, more contrast, a bit more square for reactions
// - StackChanLike: "beeps and boops" balanced, moderate pitch

static SfxDef make(float f0, float f1, uint16_t ms, float amp, Wave w) {
  SfxDef d; d.f0=f0; d.f1=f1; d.ms=ms; d.amp=amp; d.wave=w; return d;
}

static void fillDefault(SfxDef* t) {
  t[(int)SfxId::Ready]    = make( 880,  880, 120, 0.33f, Wave::Sine);
  t[(int)SfxId::ChirpUp]  = make( 520, 1120, 160, 0.42f, Wave::Sine);
  t[(int)SfxId::ChirpMid] = make( 640,  780, 120, 0.22f, Wave::Sine);
  t[(int)SfxId::Pop]      = make(1300,  520,  95, 0.42f, Wave::Square);
  t[(int)SfxId::Sigh]     = make( 420,  240, 280, 0.22f, Wave::Sine);
  t[(int)SfxId::Scared]   = make(1500,  720, 180, 0.55f, Wave::Square);
  t[(int)SfxId::Down]     = make( 520,  220, 190, 0.28f, Wave::Sine);
  t[(int)SfxId::Annoyed]  = make( 980,  320, 120, 0.40f, Wave::Square);
  t[(int)SfxId::Think]    = make( 620,  940, 240, 0.20f, Wave::Sine);
  t[(int)SfxId::Confirm]  = make( 740,  990, 150, 0.26f, Wave::Sine);
  t[(int)SfxId::Soft]     = make( 520,  650, 120, 0.16f, Wave::Sine);
  t[(int)SfxId::Wake]     = make( 460,  820, 220, 0.30f, Wave::Sine);
}

static void fillCute(SfxDef* t) {
  fillDefault(t);
  t[(int)SfxId::Ready]    = make( 990,  990,  95, 0.28f, Wave::Sine);
  t[(int)SfxId::ChirpUp]  = make( 620, 1480, 160, 0.45f, Wave::Sine);
  t[(int)SfxId::ChirpMid] = make( 720,  980, 120, 0.20f, Wave::Sine);
  t[(int)SfxId::Pop]      = make(1700,  700,  85, 0.38f, Wave::Square);
  t[(int)SfxId::Sigh]     = make( 380,  240, 320, 0.18f, Wave::Sine);
  t[(int)SfxId::Scared]   = make(1900,  880, 150, 0.50f, Wave::Square);
  t[(int)SfxId::Annoyed]  = make(1100,  420, 110, 0.35f, Wave::Square);
  t[(int)SfxId::Think]    = make( 720, 1080, 220, 0.18f, Wave::Sine);
  t[(int)SfxId::Confirm]  = make( 860, 1160, 120, 0.22f, Wave::Sine);
  t[(int)SfxId::Soft]     = make( 620,  820,  90, 0.14f, Wave::Sine);
  t[(int)SfxId::Wake]     = make( 620, 1060, 180, 0.26f, Wave::Sine);
}

static void fillSerious(SfxDef* t) {
  fillDefault(t);
  t[(int)SfxId::Ready]    = make( 740,  740, 120, 0.24f, Wave::Sine);
  t[(int)SfxId::ChirpUp]  = make( 420,  820, 160, 0.30f, Wave::Sine);
  t[(int)SfxId::ChirpMid] = make( 520,  620, 120, 0.16f, Wave::Sine);
  t[(int)SfxId::Pop]      = make(1100,  520, 100, 0.34f, Wave::Square);
  t[(int)SfxId::Scared]   = make(1300,  620, 180, 0.42f, Wave::Square);
  t[(int)SfxId::Annoyed]  = make( 860,  320, 120, 0.36f, Wave::Square);
  t[(int)SfxId::Think]    = make( 520,  760, 260, 0.18f, Wave::Sine);
  t[(int)SfxId::Confirm]  = make( 620,  820, 160, 0.20f, Wave::Sine);
  t[(int)SfxId::Wake]     = make( 520,  760, 220, 0.24f, Wave::Sine);
}

static void fillEmo(SfxDef* t) {
  fillDefault(t);
  t[(int)SfxId::ChirpUp]  = make( 520, 1380, 170, 0.48f, Wave::Sine);
  t[(int)SfxId::ChirpMid] = make( 620,  900, 140, 0.22f, Wave::Sine);
  t[(int)SfxId::Pop]      = make(1800,  600,  85, 0.45f, Wave::Square);
  t[(int)SfxId::Sigh]     = make( 480,  220, 320, 0.22f, Wave::Sine);
  t[(int)SfxId::Scared]   = make(2100,  820, 160, 0.62f, Wave::Square);
  t[(int)SfxId::Annoyed]  = make(1200,  260, 120, 0.45f, Wave::Square);
  t[(int)SfxId::Think]    = make( 740, 1160, 240, 0.20f, Wave::Sine);
  t[(int)SfxId::Confirm]  = make( 880, 1240, 130, 0.25f, Wave::Sine);
}

static void fillStackChan(SfxDef* t) {
  fillDefault(t);
  t[(int)SfxId::Ready]    = make( 880,  880, 100, 0.30f, Wave::Square);
  t[(int)SfxId::ChirpUp]  = make( 660, 1320, 140, 0.38f, Wave::Sine);
  t[(int)SfxId::ChirpMid] = make( 720,  860, 110, 0.20f, Wave::Sine);
  t[(int)SfxId::Pop]      = make(1500,  620,  90, 0.40f, Wave::Square);
  t[(int)SfxId::Scared]   = make(1700,  720, 170, 0.54f, Wave::Square);
  t[(int)SfxId::Think]    = make( 620,  980, 220, 0.18f, Wave::Sine);
  t[(int)SfxId::Confirm]  = make( 740, 1040, 140, 0.22f, Wave::Sine);
}

void applyPreset(PresetId id, AudioSfx& audio) {
  SfxDef t[(int)SfxId::Count];
  switch (id) {
    case PresetId::Cute: fillCute(t); break;
    case PresetId::Serious: fillSerious(t); break;
    case PresetId::EmoLike: fillEmo(t); break;
    case PresetId::StackChanLike: fillStackChan(t); break;
    default: fillDefault(t); break;
  }
  for (int i = 0; i < (int)SfxId::Count; i++) audio.setDef((SfxId)i, t[i]);
}
