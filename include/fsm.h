#pragma once
#include "types.h"
#include "audio_sfx.h"

class Brain {
public:
  void begin(uint32_t nowMs);
  void onEvent(const Event& e, uint32_t nowMs, AudioSfx& audio);

  State state() const { return st; }
  const Mood& moodRef() const { return mood; }
  const EmotionState& emotionRef() const { return emo; }

private:
  State st = State::Boot;
  uint32_t stateEnterAt = 0;

  uint32_t attentionUntil = 0;
  uint32_t listeningStartAt = 0;
  bool capturedAudio = false;

  // Idle micro SFX scheduler
  uint32_t nextIdleSfxAt = 0;

  Mood mood;
  EmotionState emo;

  // mood helpers
  void moodRelax1s();
  void applyMoodFromEvent(const Event& e);

  // emotion helpers
  uint8_t emoPriority(Emotion e) const;
  void setEmotion(uint32_t nowMs, Emotion e, float intensity, uint32_t ttlMs, uint32_t lockMs);
  void emotionHandleEvent(const Event& e, uint32_t nowMs, AudioSfx& audio);
  void emotionExpire(uint32_t nowMs);

  // fsm helpers
  void enterState(State next, uint32_t nowMs);
  void fsmHandleEvent(const Event& e, uint32_t nowMs, AudioSfx& audio);
};
