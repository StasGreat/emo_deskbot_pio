#include <Arduino.h>
#include "config.h"
#include "event_queue.h"
#include "inputs.h"
#include "audio_sfx.h"
#include "fsm.h"
#include "render_oled.h"

static EventQueue q;
static Inputs inputs;
static AudioSfx audio;
static Brain brain;
static Renderer renderer;

void setup() {
  Serial.begin(115200);
  delay(150);

  Serial.println();
  Serial.println("EMO DeskBot - minimal behavior + SFX + OLED");
  Serial.println("Pins:");
  Serial.printf("I2C SDA=%d SCL=%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("I2S BCLK=%d LRCK=%d DOUT=%d\n", PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  Serial.printf("Inputs PTT=%d TOUCH=%d\n", PIN_PTT, PIN_TOUCH);

  inputs.begin();

  audio.setRateLimitMs(AUDIO_SFX_RATE_LIMIT_MS);
  audio.begin();

  renderer.begin();

  uint32_t now = millis();
  brain.begin(now);

  // Startup sound
  audio.play(SfxId::Ready, now);
}

void loop() {
  uint32_t now = millis();

  // sample inputs -> events
  inputs.sample(now, q);

  // process events
  Event e;
  while (q.pop(e)) {
    brain.onEvent(e, now, audio);
  }

  // render face
  renderer.render(now, brain.state(), brain.moodRef(), brain.emotionRef());

  delay(FRAME_DELAY_MS);
}
