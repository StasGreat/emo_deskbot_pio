#include "voice_recorder.h"
#include "audio_utils.h"
#include "config.h"
#include <math.h>

void VoiceRecorder::begin(AudioI2S& i2s) {
  i2sRef = &i2s;
}

void VoiceRecorder::clear() {
  writeIndex = 0;
  playIndex = 0;
  txFrames = 0;
  txIndex = 0;
  hpfX = 0.0f;
  hpfY = 0.0f;
  agcGain = 1.0f;
  agcEnv = 0.0f;
}

void VoiceRecorder::startRecord() {
  if (!i2sRef) return;
  recording = true;
  playing = false;
  clear();
}

void VoiceRecorder::stopRecord() {
  recording = false;
}

bool VoiceRecorder::startPlayback() {
  if (!i2sRef) return false;
  if (writeIndex < (AUDIO_SAMPLE_RATE / 8)) return false; // <125ms too short
  playing = true;
  recording = false;
  playIndex = 0;
  txFrames = 0;
  txIndex = 0;
  return true;
}

uint32_t VoiceRecorder::recordedMs() const {
  return (uint32_t)((uint64_t)writeIndex * 1000ULL / (uint64_t)AUDIO_SAMPLE_RATE);
}

void VoiceRecorder::update(uint32_t nowMs) {
  (void)nowMs;
  if (!i2sRef) return;

  if (recording) {
    size_t got = i2sRef->readRx(rxBuf, kChunkFrames);
    if (got > 0) {
      const size_t ch = MIC_RIGHT_CHANNEL ? 1 : 0;
      for (size_t i = 0; i < got; i++) {
        if (writeIndex >= kMaxFrames) {
          recording = false;
          break;
        }
        int32_t s = rxBuf[i * 2 + ch];
        int16_t in = mic32ToS16(s);
        float x = (float)in;

        if (MIC_HPF_ENABLED) {
          float y = x - hpfX + MIC_HPF_ALPHA * hpfY;
          hpfX = x;
          hpfY = y;
          x = y;
        }

        if (MIC_AGC_ENABLED) {
          float ax = fabsf(x);
          agcEnv = agcEnv * MIC_AGC_ENV_DECAY + ax * (1.0f - MIC_AGC_ENV_DECAY);
          float desired = MIC_AGC_TARGET / (agcEnv + 1.0f);
          if (desired < MIC_AGC_MIN_GAIN) desired = MIC_AGC_MIN_GAIN;
          if (desired > MIC_AGC_MAX_GAIN) desired = MIC_AGC_MAX_GAIN;
          float k = (desired < agcGain) ? MIC_AGC_RELEASE : MIC_AGC_ATTACK;
          agcGain += (desired - agcGain) * k;
          x *= agcGain;
        }

        monoBuf[writeIndex++] = clamp16((int32_t)lroundf(x));
      }
    }
  }

  if (!playing) return;

  // flush pending TX
  if (txFrames > txIndex) {
    size_t toWrite = txFrames - txIndex;
    size_t written = i2sRef->writeTxNonBlocking(txBuf + (txIndex * 2), toWrite);
    txIndex += written;
    if (txIndex < txFrames) return;
    txFrames = 0;
    txIndex = 0;
  }

  if (playIndex >= writeIndex) {
    playing = false;
    return;
  }

  size_t frames = writeIndex - playIndex;
  if (frames > kChunkFrames) frames = kChunkFrames;

  for (size_t i = 0; i < frames; i++) {
    int16_t v = monoBuf[playIndex + i];
    int32_t v32 = ((int32_t)v) << 16;
    txBuf[i * 2 + 0] = v32;
    txBuf[i * 2 + 1] = v32;
  }

  playIndex += frames;
  txFrames = frames;
  txIndex = 0;

  size_t written = i2sRef->writeTxNonBlocking(txBuf, txFrames);
  txIndex = written;
}
