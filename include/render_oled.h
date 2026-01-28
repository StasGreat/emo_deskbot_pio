#pragma once
#include "types.h"

class Renderer {
public:
  void begin();
  void render(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo,
              float rollDeg, float pitchDeg);

private:
  uint32_t nextBlinkAt = 0;
  bool blink = false;

  uint32_t nextSaccadeAt = 0;
  int8_t pupilDx = 0;
  int8_t pupilDy = 0;

  // Gaze controller (pixels, float for smoothing)
  float gazeX = 0.0f;
  float gazeY = 0.0f;
  float gazeTargetX = 0.0f;
  float gazeTargetY = 0.0f;
  uint32_t attentionHoldUntil = 0;
  float holdX = 0.0f;
  float holdY = 0.0f;
  uint32_t startleUntil = 0;
  uint32_t startleFreezeUntil = 0;
  float startleKickX = 0.0f;
  float startleKickY = 0.0f;
  State lastState = State::Boot;
  Emotion lastEmotion = Emotion::Neutral;

  // Breathing idle (subtle animation)
  float breathPhase = 0.0f;
  float breathValue = 0.0f; // -1..+1
  uint32_t lastBreathAt = 0;

  void drawFace(State st, const Mood& mood, const EmotionState& emo);
  void updateIdleAnim(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo,
                      float rollDeg, float pitchDeg);
};
