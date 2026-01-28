#pragma once
#include <Arduino.h>
#include "audio_i2s.h"
#include "config.h"

// Simple dictaphone: record from I2S RX and play back to I2S TX.
class VoiceRecorder {
public:
  void begin(AudioI2S& i2s);
  void startRecord();
  void stopRecord();
  bool startPlayback();
  void update(uint32_t nowMs);

  bool isRecording() const { return recording; }
  bool isPlaying() const { return playing; }
  uint32_t recordedMs() const;
  size_t recordedFrames() const { return writeIndex; }
  void clear();

private:
  AudioI2S* i2sRef = nullptr;
  bool recording = false;
  bool playing = false;
  size_t writeIndex = 0;
  size_t playIndex = 0;
  float hpfX = 0.0f;
  float hpfY = 0.0f;
  float agcGain = 1.0f;
  float agcEnv = 0.0f;

  static const size_t kChunkFrames = 128;
  static const size_t kMaxSeconds = 3;
  static const size_t kMaxFrames = AUDIO_SAMPLE_RATE * kMaxSeconds;

  int16_t monoBuf[kMaxFrames] = {0};
  int32_t rxBuf[kChunkFrames * 2] = {0};
  int32_t txBuf[kChunkFrames * 2] = {0};
  size_t txFrames = 0;
  size_t txIndex = 0;
};
