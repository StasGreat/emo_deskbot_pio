#include "render_oled.h"
#include "config.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

// SH1106 128x64 I2C on custom pins
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

static inline int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

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

void Renderer::updateIdleAnim(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo, float rollDeg, float pitchDeg) {
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

  // Breathing idle: subtle sinusoid, slower in Sleep, faster in Attention
  if (lastBreathAt == 0) lastBreathAt = nowMs;
  float dt = (nowMs - lastBreathAt) / 1000.0f;
  lastBreathAt = nowMs;

  float breathHz = 0.18f; // ~5.5s cycle
  if (st == State::Attention) breathHz = 0.24f;
  if (st == State::Sleep) breathHz = 0.10f;
  if (st == State::Listening) breathHz = 0.14f;

  breathPhase += dt * 6.2831853f * breathHz;
  if (breathPhase > 100000.0f) breathPhase = fmodf(breathPhase, 6.2831853f);
  breathValue = sinf(breathPhase); // -1..+1

  // Saccade only in idle/attention (suppressed during startle)
  if ((st == State::Idle || st == State::Attention) && !(nowMs < startleUntil)) {
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
  // Gaze controller: IMU gaze + Attention Hold + Return-to-center + Startle
  // 1) Compute IMU desired gaze (pixels)
  const float maxDeg = 35.0f; // clamp degrees
  float r = rollDeg;
  float p = pitchDeg;
  if (r > maxDeg) r = maxDeg;
  if (r < -maxDeg) r = -maxDeg;
  if (p > maxDeg) p = maxDeg;
  if (p < -maxDeg) p = -maxDeg;
  float imuX = (r / maxDeg) * 4.0f; // 35deg -> 4px
  float imuY = (p / maxDeg) * 3.0f; // 35deg -> 3px

  // 2) Startle trigger on emotion transition to Surprised/Scared
  bool startleNow = false;
  if ((emo.cur == Emotion::Surprised || emo.cur == Emotion::Scared) && lastEmotion != emo.cur) {
    startleNow = true;
  }
  if (startleNow) {
    startleUntil = nowMs + 320;
    startleFreezeUntil = nowMs + 120;
    // Kick pupils opposite to current direction of interest
    startleKickX = clamp((int)lroundf(-imuX * 1.6f), -5, 5);
    startleKickY = clamp((int)lroundf(-imuY * 1.6f), -4, 4);
    gazeX = startleKickX;
    gazeY = startleKickY;
  }

  // Eye contact hold after speaking (briefly look at user)
  if (st == State::Idle && lastState == State::Speaking) {
    attentionHoldUntil = nowMs + 900;
    holdX = 0.0f;
    holdY = 0.0f;
  }

  // 3) Attention Hold on entering ATTENTION (usually due to Tilt)
  if (st == State::Attention && lastState != State::Attention) {
    // capture current IMU gaze as point-of-interest
    attentionHoldUntil = nowMs + 1500;
    holdX = imuX;
    holdY = imuY;
  }

  // Select gaze target
  if (nowMs < attentionHoldUntil) {
    gazeTargetX = holdX;
    gazeTargetY = holdY;
  } else {
    // normal gaze follows IMU
    gazeTargetX = imuX;
    gazeTargetY = imuY;
  }

  // 4) Return-to-center via smoothing
  float alpha = 0.08f;
  if (st == State::Listening) alpha = 0.10f;
  if (st == State::Processing || st == State::Speaking) alpha = 0.05f;

  // During startle: freeze then recover
  if (nowMs < startleUntil) {
    if (nowMs < startleFreezeUntil) {
      // keep kick (freeze)
    } else {
      // faster recovery
      gazeX += (gazeTargetX - gazeX) * 0.18f;
      gazeY += (gazeTargetY - gazeY) * 0.18f;
    }
  } else {
    gazeX += (gazeTargetX - gazeX) * alpha;
    gazeY += (gazeTargetY - gazeY) * alpha;
  }

  // breath wiggle: tiny vertical micro-motion (very subtle)
  float bw = 0.35f;
  if (st == State::Sleep) bw = 0.15f;
  gazeY += breathValue * bw;

  // Convert to integer pupil offsets and blend with saccade
  int gx = clamp((int)lroundf(gazeX), -5, 5);
  int gy = clamp((int)lroundf(gazeY), -4, 4);
  pupilDx = (int8_t)clamp(gx + pupilDx, -5, 5);
  pupilDy = (int8_t)clamp(gy + pupilDy, -4, 4);

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
  bool startleActive = (millis() < startleUntil);
  bool wide = (st == State::Attention || emo.cur == Emotion::Surprised || emo.cur == Emotion::Scared || startleActive);
  bool squint = (emo.cur == Emotion::Angry || emo.cur == Emotion::Happy);
  bool sleepy = (st == State::Sleep || emo.cur == Emotion::Sleepy);
  bool droop = (emo.cur == Emotion::Sad);

  // Breathing offsets (subtle)
  int breathBrowOffset = 0;
  int breathMouthOffset = 0;
  if (st == State::Idle || st == State::Attention || st == State::Listening) {
    breathBrowOffset = (int)lroundf(breathValue * 1.0f);
    breathMouthOffset = (int)lroundf(breathValue * 1.0f);
  }
  if (st == State::Sleep) {
    breathBrowOffset = (int)lroundf(breathValue * 1.0f);
    breathMouthOffset = (int)lroundf(breathValue * 0.5f);
  }

  // Eyebrows
  int browTilt = 0;
  if (emo.cur == Emotion::Angry) browTilt = -4;
  if (emo.cur == Emotion::Surprised) browTilt = 4;
  if (emo.cur == Emotion::Sad) browTilt = 2;

  // Left brow line
  u8g2.drawLine(EYE_LX, BROW_Y + breathBrowOffset, EYE_LX + 20, BROW_Y + browTilt + breathBrowOffset);
  // Right brow line
  u8g2.drawLine(EYE_RX, BROW_Y + browTilt + breathBrowOffset, EYE_RX + 20, BROW_Y + breathBrowOffset);

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
  int mouthY = MOUTH_Y + breathMouthOffset;
  if (st == State::Speaking) {
    // simple mouth loop based on time
    int phase = (millis() / 120) % 4;
    if (phase == 0) u8g2.drawFrame(44, mouthY, 40, 10);
    if (phase == 1) u8g2.drawFrame(44, mouthY, 40, 14);
    if (phase == 2) u8g2.drawFrame(52, mouthY, 24, 14);
    if (phase == 3) u8g2.drawLine(44, mouthY + 8, 84, mouthY + 8);
  } else if (emo.cur == Emotion::Happy) {
    u8g2.drawEllipse(64, mouthY + 8, 18, 10,
                     U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  } else if (emo.cur == Emotion::Sad) {
    u8g2.drawEllipse(64, mouthY + 12, 18, 10,
                     U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  } else if (emo.cur == Emotion::Surprised || emo.cur == Emotion::Scared) {
    u8g2.drawFrame(56, mouthY, 16, 16);
  } else if (emo.cur == Emotion::Angry) {
    u8g2.drawLine(44, mouthY + 8, 84, mouthY + 8);
    u8g2.drawLine(44, mouthY + 9, 84, mouthY + 9);
  } else {
    u8g2.drawLine(48, mouthY + 8, 80, mouthY + 8);
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

void Renderer::render(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo, float rollDeg, float pitchDeg) {
  updateIdleAnim(nowMs, st, mood, emo, rollDeg, pitchDeg);
  lastState = st;
  lastEmotion = emo.cur;
  u8g2.clearBuffer();
  drawFace(st, mood, emo);
  u8g2.sendBuffer();
}
