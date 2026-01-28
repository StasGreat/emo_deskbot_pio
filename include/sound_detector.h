#pragma once
#include <Arduino.h>
#include "types.h"
#include "event_queue.h"
#include "audio_i2s.h"

class SoundDetector {
public:
  void begin(AudioI2S& i2s);

  // Call often. Emits SoundPeak events.
  // currentState suppresses peaks while speaking/listening to reduce self-triggering.
  void update(uint32_t nowMs, State currentState, EventQueue& q);

  // Tuning
  void setPeakThreshold(float v) { peakThreshold = v; }      // 0..1
  void setNoiseAdapt(float v) { noiseAdapt = v; }            // 0..1
  void setMinIntervalMs(uint32_t v) { minIntervalMs = v; }

private:
  AudioI2S* i2sRef = nullptr;

  float noiseFloor = 0.02f;     // RMS baseline 0..1
  float peakThreshold = 0.35f;  // trigger threshold
  float noiseAdapt = 0.01f;     // noise floor adaptation when quiet
  uint32_t minIntervalMs = 600;
  uint32_t lastPeakAt = 0;

  float readRmsChunk();
};
