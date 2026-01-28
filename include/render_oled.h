#pragma once
#include "types.h"

class Renderer {
public:
  void begin();
  void render(uint32_t nowMs, State st, const Mood& mood, const EmotionState& emo);

private:
  uint32_t nextBlinkAt = 0;
  bool blink = false;

  uint32_t nextSaccadeAt = 0;
  int8_t pupilDx = 0;
  int8_t pupilDy = 0;

  void drawFace(State st, const Mood& mood, const EmotionState& emo);
  void updateIdleAnim(uint32_t nowMs, State st, const Mood& mood);
};
