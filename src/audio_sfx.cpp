#include "audio_sfx.h"
#include "config.h"
#include <math.h>

enum class Wave : uint8_t { Sine = 0, Square = 1, Noise = 2 };

struct SfxDef {
  float f0;
  float f1;
  uint16_t ms;
  float amp;   // 0..1
  Wave wave;
};

static const int SAMPLE_RATE = AUDIO_SAMPLE_RATE;

static const SfxDef SFX_TABLE[(int)SfxId::Count] = {
  /*Ready*/   { 880,  880, 120, 0.35f, Wave::Sine   },
  /*ChirpUp*/ { 520, 1040, 140, 0.40f, Wave::Sine   },
  /*ChirpMid*/{ 600,  760, 120, 0.25f, Wave::Sine   },
  /*Pop*/     {1200,  400,  90, 0.45f, Wave::Square },
  /*Sigh*/    { 420,  260, 260, 0.25f, Wave::Sine   },
  /*Scared*/  {1400,  900, 160, 0.55f, Wave::Square },
  /*Down*/    { 520,  220, 180, 0.30f, Wave::Sine   },
  /*Annoyed*/ { 900,  300, 110, 0.40f, Wave::Square },
};

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

void AudioSfx::begin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  // Prefer modern define if available
  #ifdef I2S_COMM_FORMAT_STAND_I2S
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  #else
    cfg.communication_format = I2S_COMM_FORMAT_I2S;
  #endif
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 6;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(port, &cfg, 0, nullptr);
  i2s_set_pin(port, &pins);
  i2s_zero_dma_buffer(port);
}

void AudioSfx::stop() {
  i2s_zero_dma_buffer(port);
}

void AudioSfx::play(SfxId id, uint32_t nowMs) {
  // rate limit
  if (nowMs - lastSfxAt < rateLimitMs) return;
  lastSfxAt = nowMs;
  playBlocking(id);
}

void AudioSfx::playBlocking(SfxId id) {
  const SfxDef& d = SFX_TABLE[(int)id];
  const uint32_t total = (uint32_t)((uint64_t)d.ms * (uint64_t)SAMPLE_RATE / 1000ULL);
  if (total < 20) return;

  static int16_t buf[512 * 2]; // 512 stereo frames
  float phase = 0.0f;
  const float twoPi = 2.0f * 3.1415926535f;

  uint32_t n = 0;
  while (n < total) {
    uint32_t frames = total - n;
    if (frames > 512) frames = 512;

    for (uint32_t i = 0; i < frames; i++) {
      uint32_t k = n + i;
      float t = (float)k / (float)total;
      float f = d.f0 + (d.f1 - d.f0) * t;

      float env = envAD(k, total);
      float s = waveSample(d.wave, phase) * d.amp * env;

      if (s > 1.0f) s = 1.0f;
      if (s < -1.0f) s = -1.0f;

      int16_t v = (int16_t)(s * 32767.0f);
      buf[i * 2 + 0] = v;
      buf[i * 2 + 1] = v;

      phase += twoPi * f / (float)SAMPLE_RATE;
      if (phase > 100000.0f) phase = fmodf(phase, twoPi);
    }

    size_t bytes_written = 0;
    i2s_write(port, (const char*)buf, frames * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    n += frames;
  }
}
