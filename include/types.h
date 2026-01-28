#pragma once
#include <Arduino.h>

// ---------------- FSM ----------------
enum class State : uint8_t {
  Boot = 0,
  Idle,
  Attention,
  Listening,
  Processing,
  Speaking,
  Sleep,
  Error
};

// ---------------- Events ----------------
enum class EventType : uint8_t {
  None = 0,
  TouchDown,
  TouchUp,
  TouchLong,
  PttDown,
  PttUp,
  SoundPeak,
  Shake,
  Tilt,
  Tick1s,
  TtsEnd
};

struct Event {
  EventType type = EventType::None;
  float strength = 0.0f;      // 0..1 for sound/shake
  uint32_t durationMs = 0;    // for touch duration etc
  uint32_t at = 0;            // millis timestamp
};

// ---------------- Mood / Emotion ----------------
enum class Emotion : uint8_t {
  Neutral = 0,
  Curious,
  Happy,
  Sad,
  Angry,
  Surprised,
  Scared,
  Sleepy,
  Listening,
  Thinking
};

struct Mood {
  float valence = 0.2f; // -1..+1
  float arousal = 0.2f; // 0..1
  float energy  = 0.9f; // 0..1
};

struct EmotionState {
  Emotion cur = Emotion::Neutral;
  float intensity = 0.2f;      // 0..1
  uint32_t expireAt = 0;
  uint32_t lockUntil = 0;
};
