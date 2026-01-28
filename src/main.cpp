#include <Arduino.h>
#include "config.h"
#include "event_queue.h"
#include "inputs.h"
#include "audio_i2s.h"
#include "audio_sfx.h"
#include "sound_detector.h"
#include "imu_bmi270.h"
#include "fsm.h"
#include "render_oled.h"
#include "serial_console.h"

static EventQueue q;
static Inputs inputs;

static AudioI2S audioI2S;
static AudioSfx sfx;
static SoundDetector soundDet;
static ImuBmi270 imu;

static Brain brain;
static Renderer renderer;
static SerialConsole console;

void setup() {
  Serial.begin(115200);
  delay(150);

  Serial.println();
  Serial.println("EMO DeskBot - behavior + SFX + OLED + INMP441 peak detector");
  Serial.println("Pins:");
  Serial.printf("I2C SDA=%d SCL=%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("I2S BCLK=%d LRCK=%d DOUT=%d DIN=%d\n", PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, PIN_I2S_DIN);
  Serial.printf("Inputs PTT=%d TOUCH=%d\n", PIN_PTT, PIN_TOUCH);

  // Serial sound-tuning console
  console.begin(Serial, sfx);

  inputs.begin();

  audioI2S.begin();
  sfx.setRateLimitMs(AUDIO_SFX_RATE_LIMIT_MS);
  sfx.begin(audioI2S);

  soundDet.begin(audioI2S);
  soundDet.setPeakThreshold(0.35f);
  soundDet.setMinIntervalMs(600);

  bool imuOk = imu.begin();
  Serial.printf("BMI270 init: %s\n", imuOk ? "OK" : "FAIL");
  imu.setShakeThresholdG(1.6f);
  imu.setShakeMinIntervalMs(500);
  imu.setTiltThresholdDeg(25.0f);
  imu.setTiltMinIntervalMs(800);

  renderer.begin();

  uint32_t now = millis();
  brain.begin(now);

  sfx.play(SfxId::Ready, now);
}

void loop() {
  uint32_t now = millis();

  console.update(now);
  sfx.update(now);

  inputs.sample(now, q);                    // touch + ptt
  soundDet.update(now, brain.state(), q);   // INMP441 -> SoundPeak
  imu.update(now, brain.state(), q);         // BMI270 -> Shake/Tilt

  Event e;
  while (q.pop(e)) {
    brain.onEvent(e, now, sfx);
  }

  renderer.render(now, brain.state(), brain.moodRef(), brain.emotionRef(),
                  imu.rollDeg(), imu.pitchDeg());

  delay(FRAME_DELAY_MS);
}
