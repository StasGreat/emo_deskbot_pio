#include "audio_sfx.h"
#include "config.h"
#include <math.h>

static const int SAMPLE_RATE = AUDIO_SAMPLE_RATE;

static const SfxDef DEFAULT_SFX[(int)SfxId::Count] = {
  // UI / state
  /*Ready*/    { 880,  880, 120, 0.33f, Wave::Sine   }, // short beep
  // Emotions
  /*ChirpUp*/  { 520, 1120, 160, 0.42f, Wave::Sine   }, // happy up-chirp
  /*ChirpMid*/ { 640,  780, 120, 0.22f, Wave::Sine   }, // neutral/curious
  /*Pop*/      {1300,  520,  95, 0.42f, Wave::Square }, // surprised pop
  /*Sigh*/     { 420,  240, 280, 0.22f, Wave::Sine   }, // sleepy/sad
  /*Scared*/   {1500,  720, 180, 0.55f, Wave::Square }, // alarm-ish
  /*Down*/     { 520,  220, 190, 0.28f, Wave::Sine   }, // cancel / timeout
  /*Annoyed*/  { 980,  320, 120, 0.40f, Wave::Square }, // annoyed
  // Extra motifs
  /*Think*/    { 620,  940, 240, 0.20f, Wave::Sine   }, // thinking sweep (soft)
  /*Confirm*/  { 740,  990, 150, 0.26f, Wave::Sine   }, // acknowledgement
  /*Soft*/     { 520,  650, 120, 0.16f, Wave::Sine   }, // gentle tiny chirp
  /*Wake*/     { 460,  820, 220, 0.30f, Wave::Sine   }, // wake-up/attention
};

static SfxDef sfxDefs[(int)SfxId::Count];

static uint32_t rng_state = 0x12345678u;
static inline float frand01() {
  rng_state = rng_state * 1664525u + 1013904223u;
  return (rng_state >> 8) * (1.0f / 16777216.0f);
}

static inline float envAD(uint32_t n, uint32_t total) {
  uint32_t attack = (uint32_t)(SAMPLE_RATE * 0.008f);
  if (attack < 10) attack = 10;
  if (attack > total / 4) attack = total / 4;

  if (n < attack) return (float)n / (float)attack;

  float t = (float)(n - attack) / (float)(total - attack);
  float d = 1.0f - t;
  return d * d;
}

static inline float waveSample(Wave w, float phase) {
  switch (w) {
    case Wave::Sine:   return sinf(phase);
    case Wave::Square: return (sinf(phase) >= 0) ? 1.0f : -1.0f;
    case Wave::Noise:  return (frand01() * 2.0f - 1.0f);
    default:           return 0.0f;
  }
}

void AudioSfx::begin(AudioI2S& i2s) {
  resetDefaults();
  i2sRef = &i2s;
  i2sRef->begin();
}

SfxDef AudioSfx::getDef(SfxId id) const {
  return sfxDefs[(int)id];
}

void AudioSfx::setDef(SfxId id, const SfxDef& def) {
  sfxDefs[(int)id] = def;
}

void AudioSfx::resetDefaults() {
  for (int i = 0; i < (int)SfxId::Count; i++) sfxDefs[i] = DEFAULT_SFX[i];
}

void AudioSfx::dumpDefs(Stream& out) const {
  out.println("SFX defs:");
  for (int i = 0; i < (int)SfxId::Count; i++) {
    const SfxDef& d = sfxDefs[i];
    out.printf("  %02d: f0=%.1f f1=%.1f ms=%u amp=%.2f wave=%u\n", i, d.f0, d.f1, (unsigned)d.ms, d.amp, (unsigned)d.wave);
  }
}

void AudioSfx::stop() {
  playing = false;
  bufFrames = 0;
  bufIndex = 0;
  if (i2sRef) i2sRef->stop();
}

void AudioSfx::playPriority(SfxId id, uint8_t priority, uint32_t nowMs) {
  if (!i2sRef || externalBusy) return;
  uint32_t minGap = rateLimitMs;
  if (priority == 0) minGap = rateLimitMs * 2;
  if (priority >= 2) minGap = 0;
  if (minGap && (nowMs - lastSfxAt < minGap)) return;

  if (playing) {
    if (priority < 2 && priority <= curPriority) return;
    stop(); // preempt current
  }

  curDef = sfxDefs[(int)id];
  totalFrames = (uint32_t)((uint64_t)curDef.ms * (uint64_t)SAMPLE_RATE / 1000ULL);
  if (totalFrames < 20) return;

  framesDone = 0;
  phase = 0.0f;
  bufFrames = 0;
  bufIndex = 0;
  playing = true;
  curPriority = priority;
  lastSfxAt = nowMs;
}

void AudioSfx::play(SfxId id, uint32_t nowMs) {
  playPriority(id, 1, nowMs);
}

void AudioSfx::update(uint32_t nowMs) {
  (void)nowMs;
  if (!playing || !i2sRef) return;

  // Flush pending frames first.
  if (bufFrames > bufIndex) {
    size_t toWrite = bufFrames - bufIndex;
    size_t written = i2sRef->writeTxNonBlocking(buf + (bufIndex * 2), toWrite);
    bufIndex += written;
    if (bufIndex < bufFrames) return;
    bufFrames = 0;
    bufIndex = 0;
  }

  if (framesDone >= totalFrames) {
    playing = false;
    return;
  }

  size_t frames = (size_t)(totalFrames - framesDone);
  if (frames > kChunkFrames) frames = kChunkFrames;

  const float twoPi = 2.0f * 3.1415926535f;
  uint32_t start = framesDone;
  for (size_t i = 0; i < frames; i++) {
    uint32_t k = start + (uint32_t)i;
    float t = (float)k / (float)totalFrames;
    float f = curDef.f0 + (curDef.f1 - curDef.f0) * t;

    float env = envAD(k, totalFrames);
    float s = waveSample(curDef.wave, phase) * curDef.amp * env;

    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;

    int32_t v = (int32_t)(s * 2147483647.0f);
    buf[i * 2 + 0] = v;
    buf[i * 2 + 1] = v;

    phase += twoPi * f / (float)SAMPLE_RATE;
    if (phase > 100000.0f) phase = fmodf(phase, twoPi);
  }

  framesDone += (uint32_t)frames;
  bufFrames = frames;
  bufIndex = 0;

  size_t written = i2sRef->writeTxNonBlocking(buf, bufFrames);
  bufIndex = written;
}
