#pragma once
#include <Arduino.h>
#include "driver/i2s.h"

// Single I2S driver instance configured for TX (MAX98357A) + RX (INMP441) using shared BCLK/LRCK.
//
// Notes:
// - Uses 32-bit samples to accommodate INMP441 typical output format.
// - TX writes stereo interleaved (L,R) int32_t. If AUDIO_I2S_MONO=1, TX is downmixed to mono.
// - RX reads int32_t samples (stereo frames). If AUDIO_I2S_MONO=1, mono input is duplicated to L/R.

class AudioI2S {
public:
  void begin();
  void stop();

  // TX
  bool writeTx(const int32_t* stereoFrames, size_t frameCount);
  // TX (non-blocking). Returns frames actually written.
  size_t writeTxNonBlocking(const int32_t* stereoFrames, size_t frameCount);
  void setTxGain(float g) { txGain = (g < 0.0f) ? 0.0f : (g > 1.5f ? 1.5f : g); }
  float getTxGain() const { return txGain; }

  // RX (non-blocking). Returns number of frames actually read (can be 0).
  size_t readRx(int32_t* stereoFrames, size_t maxFrames);

  i2s_port_t port() const { return p; }

private:
  i2s_port_t p = I2S_NUM_0;
  bool started = false;
  float txGain = 1.0f;
  static const size_t kTmpFrames = 256;
  int32_t txTmp[kTmpFrames * 2] = {0};
  int32_t txMonoTmp[kTmpFrames] = {0};
  int32_t rxMonoTmp[kTmpFrames] = {0};
};
