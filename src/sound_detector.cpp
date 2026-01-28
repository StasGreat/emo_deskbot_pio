#include "sound_detector.h"
#include <math.h>

static const size_t CHUNK_FRAMES = 256;

static inline float sampleToFloat(int32_t s) {
  // INMP441 is commonly 24-bit data in 32-bit container (often left aligned).
  // Convert to float roughly in [-1..1]. If your signal is too weak/strong, adjust shift.
  const float denom = 8388608.0f; // 2^23
  int32_t v = (s >> 8);           // bring 24-bit to signed range
  return (float)v / denom;
}

void SoundDetector::begin(AudioI2S& i2s) {
  i2sRef = &i2s;
}

float SoundDetector::readRmsChunk() {
  if (!i2sRef) return 0.0f;

  static int32_t frames[CHUNK_FRAMES * 2];
  size_t got = i2sRef->readRx(frames, CHUNK_FRAMES);
  if (got == 0) return 0.0f;

  double acc = 0.0;
  for (size_t i = 0; i < got; i++) {
    float l = sampleToFloat(frames[i*2 + 0]);
    float r = sampleToFloat(frames[i*2 + 1]);
    float x = (fabsf(l) > fabsf(r)) ? l : r;
    acc += (double)x * (double)x;
  }

  float rms = sqrtf((float)(acc / (double)got));
  if (rms < 0.0f) rms = 0.0f;
  if (rms > 1.0f) rms = 1.0f;
  return rms;
}

void SoundDetector::update(uint32_t nowMs, State currentState, EventQueue& q) {
  // Suppress peaks while speaking/listening (speaker bleed).
  if (currentState == State::Speaking || currentState == State::Listening) {
    float rms = readRmsChunk();
    if (rms > 0.0f) {
      noiseFloor = noiseFloor * 0.999f + rms * 0.001f;
      if (noiseFloor < 0.005f) noiseFloor = 0.005f;
      if (noiseFloor > 0.3f) noiseFloor = 0.3f;
    }
    return;
  }

  float rms = readRmsChunk();
  if (rms <= 0.0f) return;

  // Adapt noise floor only when quiet
  if (rms < noiseFloor * 1.5f) {
    noiseFloor = noiseFloor * (1.0f - noiseAdapt) + rms * noiseAdapt;
    if (noiseFloor < 0.005f) noiseFloor = 0.005f;
    if (noiseFloor > 0.3f) noiseFloor = 0.3f;
  }

  float strength = (rms - noiseFloor) / fmaxf(0.05f, (0.6f - noiseFloor));
  if (strength < 0.0f) strength = 0.0f;
  if (strength > 1.0f) strength = 1.0f;

  if (strength >= peakThreshold) {
    if (nowMs - lastPeakAt >= minIntervalMs) {
      lastPeakAt = nowMs;
      q.push({EventType::SoundPeak, strength, 0, nowMs});
    }
  }
}
