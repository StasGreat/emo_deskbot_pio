#pragma once
#include <Arduino.h>
#include "audio_sfx.h"
#include "sfx_presets.h"
#include "sfx_storage.h"

// Simple line-based Serial console for tuning SFX at runtime.
// Commands:
// - help
// - sfx list
// - sfx dump
// - sfx play <id> [prio]
// - sfx set <id> f0 <Hz>
// - sfx set <id> f1 <Hz>
// - sfx set <id> ms <duration_ms>
// - sfx set <id> amp <0..1>
// - sfx set <id> wave <sine|square|noise|0|1|2>
// - sfx reset
// - sfx rate <ms>
//
// Notes:
// - Send commands with newline (Serial Monitor: Newline or Both NL&CR).

class SerialConsole {
public:
  void begin(Stream& s, AudioSfx& a, AudioI2S& out);
  void update(uint32_t nowMs);
  enum class TestMode : uint8_t { Off = 0, All, Mic, Imu, Sound };
  bool isTestMode() const { return testMode != TestMode::Off; }
  bool isTestAll() const { return testMode == TestMode::All; }
  bool isTestMic() const { return testMode == TestMode::Mic; }
  bool isTestImu() const { return testMode == TestMode::Imu; }
  bool isTestSound() const { return testMode == TestMode::Sound; }
  TestMode testModeValue() const { return testMode; }
  bool isMicMonitor() const { return micMonitorEnabled; }

private:
  Stream* io = nullptr;
  AudioSfx* audio = nullptr;
  AudioI2S* out = nullptr;
  char buf[160] = {0};
  uint8_t len = 0;
  TestMode testMode = TestMode::Off;
  bool micMonitorEnabled = false;

  void handleLine(const char* line, uint32_t nowMs);
  static bool tokenEq(const char* a, const char* b);
  static Wave parseWave(const char* s, bool* ok);
  static float parseFloat(const char* s, bool* ok);
  static long parseLong(const char* s, bool* ok);
  void printTestHelp() const;
  bool parseSfxId(const char* s, SfxId* outId) const;

  PresetId currentPreset = PresetId::Default;
  SfxStorage storage;
};
