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

// ---------------- Behavior tuning ----------------
#define ATTENTION_DEFAULT_MS 900
#define LISTENING_MAX_MS 15000

// Render FPS (rough)
#define FRAME_DELAY_MS 20
