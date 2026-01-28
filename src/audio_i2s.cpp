#include "audio_i2s.h"
#include "config.h"

void AudioI2S::begin() {
  if (started) return;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  #if AUDIO_I2S_MONO
  cfg.channel_format = MIC_RIGHT_CHANNEL ? I2S_CHANNEL_FMT_ONLY_RIGHT : I2S_CHANNEL_FMT_ONLY_LEFT;
  #else
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // stereo
  #endif
#ifndef I2S_COMM_FORMAT_STAND_I2S
#define I2S_COMM_FORMAT_STAND_I2S ((i2s_comm_format_t)0x01)
#endif
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_desc_num = 6;
  cfg.dma_frame_num = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_BCLK;
  pins.ws_io_num = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num  = PIN_I2S_DIN;

  i2s_driver_install(p, &cfg, 0, nullptr);
  i2s_set_pin(p, &pins);
  i2s_zero_dma_buffer(p);
  #if AUDIO_I2S_MONO
  i2s_set_clk(p, AUDIO_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
  #endif

  started = true;
}

void AudioI2S::stop() {
  if (!started) return;
  i2s_zero_dma_buffer(p);
}

bool AudioI2S::writeTx(const int32_t* stereoFrames, size_t frameCount) {
  if (!started) return false;
  #if AUDIO_I2S_MONO
  size_t writtenFrames = 0;
  while (writtenFrames < frameCount) {
    size_t chunk = frameCount - writtenFrames;
    if (chunk > kTmpFrames) chunk = kTmpFrames;

    for (size_t i = 0; i < chunk; i++) {
      float v = (float)stereoFrames[(writtenFrames + i) * 2] * txGain;
      if (v > 2147483647.0f) v = 2147483647.0f;
      if (v < -2147483647.0f) v = -2147483647.0f;
      txMonoTmp[i] = (int32_t)v;
    }

    size_t bytesWritten = 0;
    const size_t bytes = chunk * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)txMonoTmp, bytes, &bytesWritten, portMAX_DELAY);
    if (err != ESP_OK) return false;
    writtenFrames += bytesWritten / sizeof(int32_t);
  }
  return writtenFrames == frameCount;
  #else
  if (txGain >= 0.999f) {
    size_t bytesWritten = 0;
    const size_t bytes = frameCount * 2 * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)stereoFrames, bytes, &bytesWritten, portMAX_DELAY);
    return err == ESP_OK && bytesWritten == bytes;
  }

  size_t writtenFrames = 0;
  while (writtenFrames < frameCount) {
    size_t chunk = frameCount - writtenFrames;
    if (chunk > kTmpFrames) chunk = kTmpFrames;

    for (size_t i = 0; i < chunk * 2; i++) {
      float v = (float)stereoFrames[writtenFrames * 2 + i] * txGain;
      if (v > 2147483647.0f) v = 2147483647.0f;
      if (v < -2147483647.0f) v = -2147483647.0f;
      txTmp[i] = (int32_t)v;
    }

    size_t bytesWritten = 0;
    const size_t bytes = chunk * 2 * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)txTmp, bytes, &bytesWritten, portMAX_DELAY);
    if (err != ESP_OK) return false;
    writtenFrames += bytesWritten / (2 * sizeof(int32_t));
  }
  return writtenFrames == frameCount;
  #endif
}

size_t AudioI2S::writeTxNonBlocking(const int32_t* stereoFrames, size_t frameCount) {
  if (!started) return 0;
  #if AUDIO_I2S_MONO
  size_t totalWritten = 0;
  size_t remaining = frameCount;
  while (remaining > 0) {
    size_t chunk = remaining;
    if (chunk > kTmpFrames) chunk = kTmpFrames;

    for (size_t i = 0; i < chunk; i++) {
      float v = (float)stereoFrames[(totalWritten + i) * 2] * txGain;
      if (v > 2147483647.0f) v = 2147483647.0f;
      if (v < -2147483647.0f) v = -2147483647.0f;
      txMonoTmp[i] = (int32_t)v;
    }

    size_t bytesWritten = 0;
    const size_t bytes = chunk * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)txMonoTmp, bytes, &bytesWritten, 0);
    if (err != ESP_OK || bytesWritten == 0) break;
    size_t framesWritten = bytesWritten / sizeof(int32_t);
    totalWritten += framesWritten;
    remaining -= framesWritten;
    if (framesWritten < chunk) break;
  }
  return totalWritten;
  #else
  if (txGain >= 0.999f) {
    size_t bytesWritten = 0;
    const size_t bytes = frameCount * 2 * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)stereoFrames, bytes, &bytesWritten, 0);
    if (err != ESP_OK) return 0;
    return bytesWritten / (2 * sizeof(int32_t));
  }

  size_t totalWritten = 0;
  size_t remaining = frameCount;
  while (remaining > 0) {
    size_t chunk = remaining;
    if (chunk > kTmpFrames) chunk = kTmpFrames;

    for (size_t i = 0; i < chunk * 2; i++) {
      float v = (float)stereoFrames[totalWritten * 2 + i] * txGain;
      if (v > 2147483647.0f) v = 2147483647.0f;
      if (v < -2147483647.0f) v = -2147483647.0f;
      txTmp[i] = (int32_t)v;
    }

    size_t bytesWritten = 0;
    const size_t bytes = chunk * 2 * sizeof(int32_t);
    esp_err_t err = i2s_write(p, (const char*)txTmp, bytes, &bytesWritten, 0);
    if (err != ESP_OK || bytesWritten == 0) break;
    size_t framesWritten = bytesWritten / (2 * sizeof(int32_t));
    totalWritten += framesWritten;
    remaining -= framesWritten;
    if (framesWritten < chunk) break;
  }
  return totalWritten;
  #endif
}

size_t AudioI2S::readRx(int32_t* stereoFrames, size_t maxFrames) {
  if (!started) return 0;
  size_t totalRead = 0;
  while (totalRead < maxFrames) {
    size_t chunk = maxFrames - totalRead;
    if (chunk > kTmpFrames) chunk = kTmpFrames;
    size_t bytesRead = 0;
    #if AUDIO_I2S_MONO
    const size_t bytes = chunk * sizeof(int32_t);
    esp_err_t err = i2s_read(p, (char*)rxMonoTmp, bytes, &bytesRead, 0);
    if (err != ESP_OK || bytesRead == 0) break;
    size_t framesRead = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i < framesRead; i++) {
      int32_t v = rxMonoTmp[i];
      stereoFrames[(totalRead + i) * 2 + 0] = v;
      stereoFrames[(totalRead + i) * 2 + 1] = v;
    }
    totalRead += framesRead;
    if (framesRead < chunk) break;
    #else
    const size_t bytes = chunk * 2 * sizeof(int32_t);
    esp_err_t err = i2s_read(p, (char*)stereoFrames + totalRead * 2 * sizeof(int32_t), bytes, &bytesRead, 0);
    if (err != ESP_OK || bytesRead == 0) break;
    size_t framesRead = bytesRead / (2 * sizeof(int32_t));
    totalRead += framesRead;
    if (framesRead < chunk) break;
    #endif
  }
  return totalRead;
}
