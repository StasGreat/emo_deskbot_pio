#include "sound_detector.h"
#include "audio_utils.h"
#include "config.h"
#include <math.h>

static const size_t CHUNK_FRAMES = 256;

static inline float sampleToFloat(int32_t s) {
  return (float)mic32ToS16(s) / 32768.0f;
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
  const size_t ch = MIC_RIGHT_CHANNEL ? 1 : 0;
  for (size_t i = 0; i < got; i++) {
    float x = sampleToFloat(frames[i * 2 + ch]);
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
    lastRmsValue = rms;
    lastStrengthValue = 0.0f;
    if (rms > 0.0f) {
      noiseFloor = noiseFloor * 0.999f + rms * 0.001f;
      if (noiseFloor < 0.005f) noiseFloor = 0.005f;
      if (noiseFloor > 0.3f) noiseFloor = 0.3f;
    }
    return;
  }

  float rms = readRmsChunk();
  if (rms <= 0.0f) {
    lastRmsValue = 0.0f;
    lastStrengthValue = 0.0f;
    return;
  }
  lastRmsValue = rms;

  // Adapt noise floor only when quiet
  if (rms < noiseFloor * 1.5f) {
    noiseFloor = noiseFloor * (1.0f - noiseAdapt) + rms * noiseAdapt;
    if (noiseFloor < 0.005f) noiseFloor = 0.005f;
    if (noiseFloor > 0.3f) noiseFloor = 0.3f;
  }

  float strength = (rms - noiseFloor) / fmaxf(0.05f, (0.6f - noiseFloor));
  if (strength < 0.0f) strength = 0.0f;
  if (strength > 1.0f) strength = 1.0f;
  lastStrengthValue = strength;

  if (strength >= peakThreshold) {
    if (nowMs - lastPeakAt >= minIntervalMs) {
      lastPeakAt = nowMs;
      q.push({EventType::SoundPeak, strength, 0, nowMs});
    }
  }
}
