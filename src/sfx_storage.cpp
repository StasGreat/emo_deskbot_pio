#include "sfx_storage.h"
#include <Preferences.h>

static const char* NS = "emo_sfx";
static const char* KEY_VER = "ver";
static const char* KEY_PRESET = "preset";
static const char* KEY_RATE = "rate";
static const char* KEY_DEFS = "defs";

bool SfxStorage::load(AudioSfx& audio, PresetId* presetOut) {
  Preferences prefs;
  if (!prefs.begin(NS, true)) return false;

  uint32_t ver = prefs.getUInt(KEY_VER, 0);
  String presetStr = prefs.getString(KEY_PRESET, "default");
  uint32_t rate = prefs.getUInt(KEY_RATE, 700);

  if (ver != kVersion) {
    prefs.end();
    return false;
  }

  size_t need = sizeof(SfxDef) * (int)SfxId::Count;
  size_t have = prefs.getBytesLength(KEY_DEFS);
  if (have != need) {
    prefs.end();
    return false;
  }

  SfxDef tmp[(int)SfxId::Count];
  size_t read = prefs.getBytes(KEY_DEFS, tmp, need);
  prefs.end();
  if (read != need) return false;

  for (int i = 0; i < (int)SfxId::Count; i++) audio.setDef((SfxId)i, tmp[i]);
  audio.setRateLimitMs(rate);

  if (presetOut) {
    PresetId p = PresetId::Default;
    parsePreset(presetStr.c_str(), &p);
    *presetOut = p;
  }
  return true;
}

bool SfxStorage::save(const AudioSfx& audio, PresetId preset) {
  Preferences prefs;
  if (!prefs.begin(NS, false)) return false;

  prefs.putUInt(KEY_VER, kVersion);
  prefs.putString(KEY_PRESET, presetName(preset));
  prefs.putUInt(KEY_RATE, audio.getRateLimitMs());

  SfxDef tmp[(int)SfxId::Count];
  for (int i = 0; i < (int)SfxId::Count; i++) tmp[i] = audio.getDef((SfxId)i);

  size_t need = sizeof(SfxDef) * (int)SfxId::Count;
  size_t wrote = prefs.putBytes(KEY_DEFS, tmp, need);
  prefs.end();

  return wrote == need;
}

void SfxStorage::clear() {
  Preferences prefs;
  if (!prefs.begin(NS, false)) return;
  prefs.clear();
  prefs.end();
}
