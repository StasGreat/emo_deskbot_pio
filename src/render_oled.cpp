#include "render_oled.h"
#include "config.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

// SH1106 128x64 I2C on custom pins
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

static inline int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static const char* stateShort(State s) {
  switch (s) {
    case State::Boot: return "BOOT";
    case State::Idle: return "IDLE";
    case State::Attention: return "ATTN";
    case State::Listening: return "LISTN";
    case State::Processing: return "THNK";
    case State::Speaking: return "SPK";
    case State::Sleep: return "SLEEP";
    case State::Error: return "ERR";
    default: return "?";
  }
}

static const char* emoShort(Emotion e) {
  switch (e) {
    case Emotion::Neutral: return "NEUT";
    case Emotion::Curious: return "CURI";
    case Emotion::Happy: return "HAP";
    case Emotion::Sad: return "SAD";
    case Emotion::Angry: return "ANG";
    case Emotion::Surprised: return "SURP";
    case Emotion::Scared: return "SCARE";
    case Emotion::Sleepy: return "SLEEP";
    case Emotion::Listening: return "LIST";
    case Emotion::Thinking: return "THNK";
    default: return "?";
  }
}

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
  const float maxDeg = 20.0f; // clamp degrees (stronger response)
  float r = rollDeg;
  float p = pitchDeg;
  if (r > maxDeg) r = maxDeg;
  if (r < -maxDeg) r = -maxDeg;
  if (p > maxDeg) p = maxDeg;
  if (p < -maxDeg) p = -maxDeg;
  float imuX = (r / maxDeg) * 7.0f; // stronger horizontal response
  float imuY = (p / maxDeg) * 5.0f; // stronger vertical response

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

static const int EYE_W = 40;
static const int EYE_H = 28;
static const int EYE_R = 10;
static const int EYE_GAP = 8;
static const int EYE_Y_BASE = 12;
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

  // Eye geometry (from EmoBot project)
  int eyeW = EYE_W;
  int eyeH = EYE_H;
  int eyeR = EYE_R;
  int eyeY = EYE_Y_BASE;
  if (wide) { eyeH = 32; eyeY = 10; }
  else if (squint) { eyeH = 20; eyeY = 18; }
  else if (droop) { eyeH = 22; eyeY = 16; }

  int eyeLX = (128 / 2) - (EYE_GAP / 2) - eyeW;
  int eyeRX = (128 / 2) + (EYE_GAP / 2);

  int browY = eyeY - 6 + breathBrowOffset;
  // Left brow line
  u8g2.drawLine(eyeLX + 6, browY, eyeLX + eyeW - 2, browY + browTilt);
  // Right brow line
  u8g2.drawLine(eyeRX + 2, browY + browTilt, eyeRX + eyeW - 6, browY);

  // Eyes
  auto drawEye = [&](int x) {
    // White eye shape
    u8g2.setDrawColor(1);
    u8g2.drawRBox(x, eyeY, eyeW, eyeH, eyeR);

    // Pupil
    if (!sleepy && !blink) {
      int cx = x + eyeW / 2;
      int cy = eyeY + eyeH / 2;
      int pr = (int)((eyeW < eyeH ? eyeW : eyeH) * 0.18f);
      if (pr < 2) pr = 2;
      if (pr > 7) pr = 7;

      int px = cx + pupilDx;
      int py = cy + pupilDy;
      int minX = x + eyeR + pr;
      int maxX = x + eyeW - eyeR - pr;
      int minY = eyeY + eyeR + pr;
      int maxY = eyeY + eyeH - eyeR - pr;
      if (px < minX) px = minX;
      if (px > maxX) px = maxX;
      if (py < minY) py = minY;
      if (py > maxY) py = maxY;

      u8g2.setDrawColor(0);
      u8g2.drawDisc(px, py, pr);
      u8g2.setDrawColor(1);
      u8g2.drawPixel(px - pr / 2, py - pr / 2);
    }

    // Eyelids (mask top/bottom)
    float lid = 0.0f;
    if (sleepy) lid = 0.8f;
    else if (blink) lid = 1.0f;
    else if (squint) lid = 0.45f;
    else if (droop) lid = 0.25f;
    int cover = (int)(eyeH * 0.58f * lid);
    if (cover > 0) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(x - 1, eyeY - 1, eyeW + 2, cover);
      u8g2.drawBox(x - 1, eyeY + eyeH - cover, eyeW + 2, cover + 1);
      u8g2.setDrawColor(1);
    }
  };

  drawEye(eyeLX);
  drawEye(eyeRX);

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

void Renderer::render(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo,
                      float rollDeg, float pitchDeg, bool debugOverlay, float rms) {
  updateIdleAnim(nowMs, st, mood, emo, rollDeg, pitchDeg);
  lastState = st;
  lastEmotion = emo.cur;
  u8g2.clearBuffer();
  drawFace(st, mood, emo);
  if (debugOverlay) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(0, 8);
    u8g2.print("ST:");
    u8g2.print(stateShort(st));
    u8g2.print(" EM:");
    u8g2.print(emoShort(emo.cur));
    u8g2.setCursor(0, 16);
    u8g2.print("R:");
    u8g2.print((int)lroundf(rollDeg));
    u8g2.print(" P:");
    u8g2.print((int)lroundf(pitchDeg));
    u8g2.setCursor(0, 24);
    u8g2.print("RMS:");
    u8g2.print(rms, 2);
    u8g2.setFont(u8g2_font_6x10_tf);
  }
  u8g2.sendBuffer();
}
