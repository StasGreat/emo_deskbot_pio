#pragma once
#include <Arduino.h>
#include "driver/i2s.h"

// Single I2S driver instance configured for TX (MAX98357A) + RX (INMP441) using shared BCLK/LRCK.
//
// Notes:
// - Uses 32-bit samples to accommodate INMP441 typical output format.
// - TX writes stereo interleaved (L,R) int32_t.
// - RX reads int32_t samples (stereo frames). INMP441 often outputs on one channel (commonly left).

class AudioI2S {
public:
  void begin();
  void stop();

  // TX
  bool writeTx(const int32_t* stereoFrames, size_t frameCount);
  // TX (non-blocking). Returns frames actually written.
  size_t writeTxNonBlocking(const int32_t* stereoFrames, size_t frameCount);

  // RX (non-blocking). Returns number of frames actually read (can be 0).
  size_t readRx(int32_t* stereoFrames, size_t maxFrames);

  i2s_port_t port() const { return p; }

private:
  i2s_port_t p = I2S_NUM_0;
  bool started = false;
};
