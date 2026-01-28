#pragma once

// ---------------- Pins (your wiring) ----------------
// I2C: OLED + BMI270
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// I2S shared clock for MIC + AMP
#define PIN_I2S_BCLK 3
#define PIN_I2S_LRCK 4
#define PIN_I2S_DOUT 5   // to MAX98357A DIN
#define PIN_I2S_DIN  6   // from INMP441 SD (future)

// Inputs
#define PIN_PTT   7      // PTT button
#define PIN_TOUCH 10     // TTP223 OUT (HIGH when touched)

// ---------------- Input polarity ----------------
// Set to 1 if your PTT button is wired to GND and uses internal pullup (pressed = LOW).
#define PTT_ACTIVE_LOW 1

// Touch module typically outputs HIGH on touch.
#define TOUCH_ACTIVE_HIGH 1

// ---------------- Audio / SFX ----------------
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_SFX_RATE_LIMIT_MS 700
#define OUT_GAIN_DEFAULT 0.20f
#define AUDIO_SPK_VOLUME OUT_GAIN_DEFAULT
#define AUDIO_I2S_MONO 1

// Mic settings (from Emo-esp32-s3-voice-robot)
#define MIC_SHIFT 8
#define MIC_GAIN 1.0f
#define MIC_RIGHT_CHANNEL 0

// Mic processing
#define MIC_HPF_ENABLED 1
#define MIC_HPF_ALPHA 0.995f

#define MIC_AGC_ENABLED 1
#define MIC_AGC_TARGET 12000.0f
#define MIC_AGC_MIN_GAIN 0.6f
#define MIC_AGC_MAX_GAIN 6.0f
#define MIC_AGC_ATTACK 0.0020f
#define MIC_AGC_RELEASE 0.05f
#define MIC_AGC_ENV_DECAY 0.92f

// ---------------- Behavior tuning ----------------
#define ATTENTION_DEFAULT_MS 900
#define LISTENING_MAX_MS 15000
#define ATTENTION_COOLDOWN_MS 800
#define ATTENTION_TILT_MIN_STRENGTH 0.35f
#define ENABLE_IDLE_SFX 0

// IMU thresholds (from v9 presets)
#define IMU_SHAKE_THRESHOLD_G 1.6f
#define IMU_SHAKE_MIN_INTERVAL_MS 500
#define IMU_TILT_THRESHOLD_DEG 25.0f
#define IMU_TILT_MIN_INTERVAL_MS 800

// Render FPS (rough)
#define FRAME_DELAY_MS 20

// Dictaphone mode (record on PTT, play back on release)
#define DICTAPHONE_MODE 1
