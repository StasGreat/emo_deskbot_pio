#include "render_oled.h"
#include "config.h"
#include <U8g2lib.h>
#include <Wire.h>

// SH1106 128x64 I2C on custom pins
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

static inline int randRange(int a, int b) {
  if (b <= a) return a;
  return a + (esp_random() % (uint32_t)(b - a + 1));
}

void Renderer::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  nextBlinkAt = millis() + randRange(2000, 6000);
  nextSaccadeAt = millis() + randRange(800, 2000);
}

void Renderer::updateIdleAnim(uint32_t nowMs, State st, const Mood& mood) {
  // Blink timing depends on state
  int bmin = 2500, bmax = 6500;
  if (st == State::Listening) { bmin = 4500; bmax = 9000; }
  if (st == State::Processing) { bmin = 3500; bmax = 7000; }
  if (st == State::Sleep) { blink = true; return; }

  if (nowMs >= nextBlinkAt) {
    blink = true;
    nextBlinkAt = nowMs + randRange(bmin, bmax);
  } else {
    blink = false;
  }

  // Saccade only in idle/attention
  if (st == State::Idle || st == State::Attention) {
    if (nowMs >= nextSaccadeAt) {
      static const int8_t dx[] = {0, 3, -3, 0, 0, 2, -2};
      static const int8_t dy[] = {0, 0, 0, 2, -2, -2, -2};
      int i = randRange(0, 6);
      pupilDx = dx[i];
      pupilDy = dy[i];
      nextSaccadeAt = nowMs + randRange(800, 2500);
    }
  } else {
    pupilDx = 0;
    pupilDy = 0;
  }

  // Mood modifiers (simple)
  if (mood.arousal > 0.7f) {
    // more jittery look
    pupilDx = (pupilDx >= 0) ? pupilDx + 1 : pupilDx - 1;
    if (pupilDx > 4) pupilDx = 4;
    if (pupilDx < -4) pupilDx = -4;
  }
}

static const int EYE_Y = 20;
static const int EYE_LX = 24;
static const int EYE_RX = 74;
static const int BROW_Y = 10;
static const int MOUTH_Y = 46;

void Renderer::drawFace(State st, const Mood& mood, const EmotionState& emo) {
  // Base selections
  bool wide = (st == State::Attention || emo.cur == Emotion::Surprised || emo.cur == Emotion::Scared);
  bool squint = (emo.cur == Emotion::Angry || emo.cur == Emotion::Happy);
  bool sleepy = (st == State::Sleep || emo.cur == Emotion::Sleepy);
  bool droop = (emo.cur == Emotion::Sad);

  // Eyebrows
  int browTilt = 0;
  if (emo.cur == Emotion::Angry) browTilt = -4;
  if (emo.cur == Emotion::Surprised) browTilt = 4;
  if (emo.cur == Emotion::Sad) browTilt = 2;

  // Left brow line
  u8g2.drawLine(EYE_LX, BROW_Y, EYE_LX + 20, BROW_Y + browTilt);
  // Right brow line
  u8g2.drawLine(EYE_RX, BROW_Y + browTilt, EYE_RX + 20, BROW_Y);

  // Eyes
  auto drawEye = [&](int x) {
    if (sleepy) {
      u8g2.drawLine(x, EYE_Y + 8, x + 26, EYE_Y + 8);
      return;
    }
    if (blink || squint) {
      u8g2.drawLine(x, EYE_Y + 8, x + 26, EYE_Y + 8);
      if (squint) u8g2.drawLine(x + 3, EYE_Y + 10, x + 23, EYE_Y + 10);
      return;
    }
    if (wide) {
      u8g2.drawFrame(x, EYE_Y, 26, 18);
    } else if (droop) {
      u8g2.drawFrame(x, EYE_Y + 3, 26, 15);
    } else {
      u8g2.drawFrame(x, EYE_Y + 2, 26, 16);
    }
  };

  drawEye(EYE_LX);
  drawEye(EYE_RX);

  // Pupils
  if (!sleepy && !blink) {
    int pxL = EYE_LX + 10 + pupilDx;
    int pyL = EYE_Y + 7 + pupilDy;
    int pxR = EYE_RX + 10 + pupilDx;
    int pyR = EYE_Y + 7 + pupilDy;
    u8g2.drawBox(pxL, pyL, 5, 5);
    u8g2.drawBox(pxR, pyR, 5, 5);
  }

  // Mouth
  if (st == State::Speaking) {
    // simple mouth loop based on time
    int phase = (millis() / 120) % 4;
    if (phase == 0) u8g2.drawFrame(44, MOUTH_Y, 40, 10);
    if (phase == 1) u8g2.drawFrame(44, MOUTH_Y, 40, 14);
    if (phase == 2) u8g2.drawFrame(52, MOUTH_Y, 24, 14);
    if (phase == 3) u8g2.drawLine(44, MOUTH_Y + 8, 84, MOUTH_Y + 8);
  } else if (emo.cur == Emotion::Happy) {
    u8g2.drawEllipse(64, MOUTH_Y + 8, 18, 10,
                     U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  } else if (emo.cur == Emotion::Sad) {
    u8g2.drawEllipse(64, MOUTH_Y + 12, 18, 10,
                     U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  } else if (emo.cur == Emotion::Surprised || emo.cur == Emotion::Scared) {
    u8g2.drawFrame(56, MOUTH_Y, 16, 16);
  } else if (emo.cur == Emotion::Angry) {
    u8g2.drawLine(44, MOUTH_Y + 8, 84, MOUTH_Y + 8);
    u8g2.drawLine(44, MOUTH_Y + 9, 84, MOUTH_Y + 9);
  } else {
    u8g2.drawLine(48, MOUTH_Y + 8, 80, MOUTH_Y + 8);
  }

  // Status text (small, bottom)
  const char* stName = "";
  switch (st) {
    case State::Boot: stName="BOOT"; break;
    case State::Idle: stName="IDLE"; break;
    case State::Attention: stName="ATTN"; break;
    case State::Listening: stName="LISTEN"; break;
    case State::Processing: stName="THINK"; break;
    case State::Speaking: stName="SPEAK"; break;
    case State::Sleep: stName="SLEEP"; break;
    case State::Error: stName="ERROR"; break;
  }

  u8g2.setCursor(0, 62);
  u8g2.print(stName);
}

void Renderer::render(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo) {
  updateIdleAnim(nowMs, st, mood);
  u8g2.clearBuffer();
  drawFace(st, mood, emo);
  u8g2.sendBuffer();
}
