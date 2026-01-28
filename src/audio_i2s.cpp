#include "audio_i2s.h"
#include "config.h"

void AudioI2S::begin() {
  if (started) return;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  cfg.sample_rate = AUDIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; // stereo
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

  started = true;
}

void AudioI2S::stop() {
  if (!started) return;
  i2s_zero_dma_buffer(p);
}

bool AudioI2S::writeTx(const int32_t* stereoFrames, size_t frameCount) {
  if (!started) return false;
  size_t bytesWritten = 0;
  const size_t bytes = frameCount * 2 * sizeof(int32_t);
  esp_err_t err = i2s_write(p, (const char*)stereoFrames, bytes, &bytesWritten, portMAX_DELAY);
  return err == ESP_OK && bytesWritten == bytes;
}

size_t AudioI2S::writeTxNonBlocking(const int32_t* stereoFrames, size_t frameCount) {
  if (!started) return 0;
  size_t bytesWritten = 0;
  const size_t bytes = frameCount * 2 * sizeof(int32_t);
  esp_err_t err = i2s_write(p, (const char*)stereoFrames, bytes, &bytesWritten, 0);
  if (err != ESP_OK) return 0;
  return bytesWritten / (2 * sizeof(int32_t));
}

size_t AudioI2S::readRx(int32_t* stereoFrames, size_t maxFrames) {
  if (!started) return 0;
  size_t bytesRead = 0;
  const size_t bytes = maxFrames * 2 * sizeof(int32_t);
  esp_err_t err = i2s_read(p, (char*)stereoFrames, bytes, &bytesRead, 0);
  if (err != ESP_OK) return 0;
  return bytesRead / (2 * sizeof(int32_t));
}
