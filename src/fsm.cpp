#include "fsm.h"
#include "config.h"

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void Brain::begin(uint32_t nowMs) {
  mood = Mood();
  emo = EmotionState();
  enterState(State::Boot, nowMs);
}

void Brain::enterState(State next, uint32_t nowMs) {
  st = next;
  stateEnterAt = nowMs;

  if (st == State::Attention) {
    attentionUntil = nowMs + ATTENTION_DEFAULT_MS;
  }
  if (st == State::Listening) {
    listeningStartAt = nowMs;
    capturedAudio = false;
  }
}

uint8_t Brain::emoPriority(Emotion e) const {
  switch (e) {
    case Emotion::Scared: return 90;
    case Emotion::Surprised: return 80;
    case Emotion::Angry: return 70;
    case Emotion::Listening: return 60;
    case Emotion::Thinking: return 55;
    case Emotion::Happy: return 50;
    case Emotion::Curious: return 45;
    case Emotion::Sad: return 40;
    case Emotion::Sleepy: return 30;
    default: return 10;
  }
}

void Brain::setEmotion(uint32_t nowMs, Emotion e, float intensity, uint32_t ttlMs, uint32_t lockMs) {
  if (nowMs < emo.lockUntil) {
    if (emoPriority(e) <= emoPriority(emo.cur)) return;
  }
  emo.cur = e;
  emo.intensity = clampf(intensity, 0.0f, 1.0f);
  emo.expireAt = nowMs + ttlMs;
  emo.lockUntil = nowMs + lockMs;
}

void Brain::moodRelax1s() {
  mood.arousal = clampf(mood.arousal * 0.98f, 0.0f, 1.0f);
  if (st == State::Sleep) mood.energy = clampf(mood.energy + 0.005f, 0.0f, 1.0f);
  else mood.energy = clampf(mood.energy - 0.001f, 0.0f, 1.0f);
}

void Brain::applyMoodFromEvent(const Event& e) {
  switch (e.type) {
    case EventType::TouchUp:
      if (e.durationMs <= 400) {
        mood.valence = clampf(mood.valence + 0.06f, -1.0f, 1.0f);
        mood.arousal = clampf(mood.arousal + 0.04f, 0.0f, 1.0f);
      }
      break;
    case EventType::TouchLong:
      mood.valence = clampf(mood.valence + 0.04f, -1.0f, 1.0f);
      mood.energy  = clampf(mood.energy + 0.02f, 0.0f, 1.0f);
      break;
    case EventType::SoundPeak:
      mood.arousal = clampf(mood.arousal + 0.05f * e.strength, 0.0f, 1.0f);
      if (e.strength > 0.8f) mood.valence = clampf(mood.valence - 0.02f, -1.0f, 1.0f);
      break;
    case EventType::Shake:
      mood.arousal = clampf(mood.arousal + 0.12f * e.strength, 0.0f, 1.0f);
      mood.valence = clampf(mood.valence - 0.08f * e.strength, -1.0f, 1.0f);
      mood.energy  = clampf(mood.energy - 0.02f, 0.0f, 1.0f);
      break;
    default: break;
  }
}

void Brain::emotionExpire(uint32_t nowMs) {
  if (emo.expireAt && nowMs > emo.expireAt && st != State::Listening && st != State::Processing) {
    emo.cur = Emotion::Neutral;
    emo.intensity = 0.2f;
    emo.expireAt = 0;
  }
}

void Brain::emotionHandleEvent(const Event& e, uint32_t nowMs, AudioSfx& audio) {
  switch (e.type) {
    case EventType::TouchUp:
      if (e.durationMs <= 400) {
        setEmotion(nowMs, Emotion::Happy, 0.6f, 1100, 350);
        audio.play(SfxId::ChirpUp, nowMs);
      }
      break;

    case EventType::TouchLong:
      if (mood.energy < 0.25f) {
        setEmotion(nowMs, Emotion::Sleepy, 0.6f, 2000, 800);
        audio.play(SfxId::Sigh, nowMs);
      } else {
        setEmotion(nowMs, Emotion::Happy, 0.4f, 1500, 600);
      }
      break;

    case EventType::SoundPeak:
      if (e.strength > 0.75f) {
        setEmotion(nowMs, Emotion::Surprised, 0.8f, 900, 450);
        audio.play(SfxId::Pop, nowMs);
      } else {
        setEmotion(nowMs, Emotion::Curious, 0.5f, 700, 300);
      }
      break;

    case EventType::Shake:
      if (e.strength > 0.6f) {
        setEmotion(nowMs, Emotion::Scared, e.strength, 1400, 700);
        audio.play(SfxId::Scared, nowMs);
      }
      break;

    default: break;
  }
}

void Brain::fsmHandleEvent(const Event& e, uint32_t nowMs, AudioSfx& audio) {
  switch (st) {
    case State::Boot:
      if (e.type == EventType::Tick1s && (nowMs - stateEnterAt > 1000)) {
        enterState(State::Idle, nowMs);
      }
      break;

    case State::Idle:
      if (e.type == EventType::PttDown) {
        enterState(State::Listening, nowMs);
        audio.play(SfxId::Ready, nowMs);
        setEmotion(nowMs, Emotion::Listening, 0.7f, 3000, 999999);
      } else if (e.type == EventType::TouchDown || e.type == EventType::SoundPeak || e.type == EventType::Shake || e.type == EventType::Tilt) {
        enterState(State::Attention, nowMs);
      } else if (e.type == EventType::Tick1s && mood.energy < 0.15f) {
        enterState(State::Sleep, nowMs);
        setEmotion(nowMs, Emotion::Sleepy, 0.6f, 2000, 800);
      }
      break;

    case State::Attention:
      if (e.type == EventType::PttDown) {
        enterState(State::Listening, nowMs);
        audio.play(SfxId::Ready, nowMs);
        setEmotion(nowMs, Emotion::Listening, 0.7f, 3000, 999999);
      } else if (e.type == EventType::Tick1s) {
        if (nowMs >= attentionUntil) enterState(State::Idle, nowMs);
      }
      break;

    case State::Listening:
      if (e.type == EventType::PttUp) {
        capturedAudio = (nowMs - listeningStartAt > 250);
        if (capturedAudio) {
          enterState(State::Processing, nowMs);
          setEmotion(nowMs, Emotion::Thinking, 0.6f, 3000, 999999);
        } else {
          enterState(State::Idle, nowMs);
        }
        // unlock emotion on exit
        emo.lockUntil = 0;
      } else if (e.type == EventType::Tick1s && (nowMs - listeningStartAt > LISTENING_MAX_MS)) {
        enterState(State::Idle, nowMs);
        emo.lockUntil = 0;
        audio.play(SfxId::Down, nowMs);
      } else if (e.type == EventType::Shake && e.strength > 0.6f) {
        enterState(State::Attention, nowMs);
        emo.lockUntil = 0;
      }
      break;

    case State::Processing:
      // No STT/LLM yet - simulate thinking
      if (e.type == EventType::Tick1s && (nowMs - stateEnterAt > 1200)) {
        enterState(State::Speaking, nowMs);
        // speaking placeholder SFX
        audio.play(SfxId::ChirpMid, nowMs);
      }
      break;

    case State::Speaking:
      if (e.type == EventType::TouchDown) {
        // Interrupt speech (later: interrupt TTS stream)
        audio.stop();
        enterState(State::Attention, nowMs);
        audio.play(SfxId::Annoyed, nowMs);

        if (mood.valence < -0.2f || mood.arousal > 0.7f) setEmotion(nowMs, Emotion::Angry, 0.5f, 900, 500);
        else setEmotion(nowMs, Emotion::Curious, 0.4f, 700, 300);
      } else if (e.type == EventType::PttDown) {
        audio.stop();
        enterState(State::Listening, nowMs);
        audio.play(SfxId::Ready, nowMs);
      } else if (e.type == EventType::Tick1s && (nowMs - stateEnterAt > 1000)) {
        // placeholder "TTS end"
        enterState(State::Idle, nowMs);
        emo.lockUntil = 0;
      }
      break;

    case State::Sleep:
      if (e.type == EventType::TouchDown || (e.type == EventType::SoundPeak && e.strength > 0.75f) || e.type == EventType::PttDown || e.type == EventType::Shake) {
        enterState(State::Attention, nowMs);
        audio.play(SfxId::ChirpMid, nowMs);
      }
      break;

    case State::Error:
      if (e.type == EventType::Tick1s && (nowMs - stateEnterAt > 2000)) {
        enterState(State::Idle, nowMs);
      }
      break;
  }
}

void Brain::onEvent(const Event& e, uint32_t nowMs, AudioSfx& audio) {
  // 1) periodic mood relax
  if (e.type == EventType::Tick1s) moodRelax1s();

  // 2) mood deltas from event
  applyMoodFromEvent(e);

  // 3) emotion
  emotionExpire(nowMs);
  emotionHandleEvent(e, nowMs, audio);

  // 4) fsm
  fsmHandleEvent(e, nowMs, audio);
}
