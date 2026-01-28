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
#include "voice_recorder.h"

static EventQueue q;
static Inputs inputs;

static AudioI2S audioI2S;
static AudioSfx sfx;
static SoundDetector soundDet;
static ImuBmi270 imu;
static VoiceRecorder voice;

static Brain brain;
static Renderer renderer;
static SerialConsole console;
static uint32_t micSilentSince = 0;
static uint32_t micClipSince = 0;
static bool micSilentWarn = false;
static bool micClipWarn = false;
static float prevMicLevel = -1.0f;
static uint32_t lastMicLevelLogAt = 0;
static bool prevMicMonitor = false;
static bool prevMicSignal = false;
static bool prevTestAll = false;
static bool prevTestMic = false;
static bool prevTestImu = false;
static bool prevTestSound = false;
static bool imuTestPrevInit = false;
static uint32_t lastImuLogAt = 0;
static float imuPrevAx = 0.0f;
static float imuPrevAy = 0.0f;
static float imuPrevAz = 0.0f;
static float imuPrevRoll = 0.0f;
static float imuPrevPitch = 0.0f;
static bool wasVoicePlaying = false;

static Event lastEvent;
static bool lastEventValid = false;
static uint32_t lastTestLogAt = 0;
static bool testPrevInit = false;
static State prevState = State::Boot;
static Emotion prevEmotion = Emotion::Neutral;
static Mood prevMood;
static bool prevTouch = false;
static bool prevPtt = false;
static float prevRms = 0.0f;
static float prevStrength = 0.0f;
static float prevNoise = 0.0f;
static float prevAx = 0.0f;
static float prevAy = 0.0f;
static float prevAz = 0.0f;
static float prevRoll = 0.0f;
static float prevPitch = 0.0f;
static bool prevSfxPlay = false;
static uint8_t prevSfxPrio = 0;
static int prevQSize = 0;

static const char* stateName(State s) {
  switch (s) {
    case State::Boot: return "BOOT";
    case State::Idle: return "IDLE";
    case State::Attention: return "ATTN";
    case State::Listening: return "LISTEN";
    case State::Processing: return "THINK";
    case State::Speaking: return "SPEAK";
    case State::Sleep: return "SLEEP";
    case State::Error: return "ERROR";
    default: return "?";
  }
}

static const char* emoName(Emotion e) {
  switch (e) {
    case Emotion::Neutral: return "NEUTRAL";
    case Emotion::Curious: return "CURIOUS";
    case Emotion::Happy: return "HAPPY";
    case Emotion::Sad: return "SAD";
    case Emotion::Angry: return "ANGRY";
    case Emotion::Surprised: return "SURPRISED";
    case Emotion::Scared: return "SCARED";
    case Emotion::Sleepy: return "SLEEPY";
    case Emotion::Listening: return "LISTENING";
    case Emotion::Thinking: return "THINKING";
    default: return "?";
  }
}

static const char* eventName(EventType t) {
  switch (t) {
    case EventType::TouchDown: return "TouchDown";
    case EventType::TouchUp: return "TouchUp";
    case EventType::TouchLong: return "TouchLong";
    case EventType::PttDown: return "PttDown";
    case EventType::PttUp: return "PttUp";
    case EventType::SoundPeak: return "SoundPeak";
    case EventType::Shake: return "Shake";
    case EventType::Tilt: return "Tilt";
    case EventType::Tick1s: return "Tick1s";
    case EventType::TtsEnd: return "TtsEnd";
    default: return "None";
  }
}

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
  console.begin(Serial, sfx, audioI2S);

  inputs.begin();

  audioI2S.begin();
  audioI2S.setTxGain(AUDIO_SPK_VOLUME);
  sfx.setRateLimitMs(AUDIO_SFX_RATE_LIMIT_MS);
  sfx.begin(audioI2S);

  soundDet.begin(audioI2S);
  soundDet.setPeakThreshold(0.35f);
  soundDet.setMinIntervalMs(600);

  bool imuOk = imu.begin();
  Serial.printf("BMI270 init: %s\n", imuOk ? "OK" : "FAIL");
  imu.setShakeThresholdG(IMU_SHAKE_THRESHOLD_G);
  imu.setShakeMinIntervalMs(IMU_SHAKE_MIN_INTERVAL_MS);
  imu.setTiltThresholdDeg(IMU_TILT_THRESHOLD_DEG);
  imu.setTiltMinIntervalMs(IMU_TILT_MIN_INTERVAL_MS);

  renderer.begin();
  voice.begin(audioI2S);

  uint32_t now = millis();
  brain.begin(now);

  sfx.play(SfxId::Ready, now);
}

void loop() {
  uint32_t now = millis();

  console.update(now);
  sfx.update(now);
  voice.update(now);
  sfx.setExternalBusy(voice.isRecording() || voice.isPlaying());

  if (wasVoicePlaying && !voice.isPlaying()) {
    q.push({EventType::TtsEnd, 0, 0, now});
  }
  wasVoicePlaying = voice.isPlaying();

  inputs.sample(now, q);                    // touch + ptt
  soundDet.update(now, brain.state(), q);   // INMP441 -> SoundPeak
  imu.update(now, brain.state(), q);         // BMI270 -> Shake/Tilt

  const bool testAll = console.isTestAll();
  const bool testMic = console.isTestMic();
  const bool testImu = console.isTestImu();
  const bool testSound = console.isTestSound();
  const bool micMonitor = console.isMicMonitor();
  (void)testSound;

  if (micMonitor && !prevMicMonitor) {
    prevMicLevel = -1.0f;
    lastMicLevelLogAt = 0;
    prevMicSignal = false;
  }
  prevMicMonitor = micMonitor;
  if (testAll && !prevTestAll) {
    testPrevInit = false;
  }
  if (testMic && !prevTestMic) {
    prevMicLevel = -1.0f;
    lastMicLevelLogAt = 0;
    prevMicSignal = false;
    micSilentSince = 0;
    micClipSince = 0;
    micSilentWarn = false;
    micClipWarn = false;
  }
  if (testImu && !prevTestImu) {
    imuTestPrevInit = false;
    lastImuLogAt = 0;
  }
  prevTestAll = testAll;
  prevTestMic = testMic;
  prevTestImu = testImu;
  prevTestSound = testSound;

  const float rms = soundDet.lastRms();
  const float strength = soundDet.lastStrength();
  const float noise = soundDet.noiseFloorLevel();

  if (testAll || testMic || micMonitor) {
    // Mic diagnostics (log only on state change)
    if (rms < 0.002f) {
      if (micSilentSince == 0) micSilentSince = now;
    } else {
      micSilentSince = 0;
      if (micSilentWarn) {
        Serial.println("MIC OK: signal detected");
        micSilentWarn = false;
      }
    }
    if (rms > 0.90f) {
      if (micClipSince == 0) micClipSince = now;
    } else {
      micClipSince = 0;
      if (micClipWarn) {
        Serial.println("MIC OK: no clipping");
        micClipWarn = false;
      }
    }
    if (!micSilentWarn && micSilentSince > 0 && (now - micSilentSince) > 3000) {
      Serial.println("MIC WARN: no signal (check wiring/GAIN/CLK)");
      micSilentWarn = true;
    }
    if (!micClipWarn && micClipSince > 0 && (now - micClipSince) > 300) {
      Serial.println("MIC WARN: clipping (signal too hot)");
      micClipWarn = true;
    }

    if (micMonitor || testMic) {
      const float gate = fmaxf(0.02f, noise + 0.015f);
      const bool micSignal = rms > gate;
      const bool signalChanged = (micSignal != prevMicSignal);
      const float delta = fabsf(rms - prevMicLevel);

      if (signalChanged && (now - lastMicLevelLogAt >= 150)) {
        if (testMic) {
          Serial.printf("TEST MIC signal=%s rms=%.3f nf=%.3f\n", micSignal ? "ON" : "OFF", rms, noise);
        } else {
          Serial.printf("MIC signal=%s rms=%.3f nf=%.3f\n", micSignal ? "ON" : "OFF", rms, noise);
        }
        lastMicLevelLogAt = now;
      } else if (micSignal && (prevMicLevel < 0.0f || delta >= 0.05f) && (now - lastMicLevelLogAt >= 200)) {
        if (testMic) {
          Serial.printf("TEST MIC level=%.3f rms=%.3f nf=%.3f\n", rms, rms, noise);
        } else {
          Serial.printf("MIC level=%.3f rms=%.3f nf=%.3f\n", rms, rms, noise);
        }
        lastMicLevelLogAt = now;
      }

      if (signalChanged || micSignal) {
        prevMicLevel = rms;
      }
      prevMicSignal = micSignal;
    }
  }

  Event e;
  while (q.pop(e)) {
    #if DICTAPHONE_MODE
      if (e.type == EventType::PttDown) {
        voice.startRecord();
      } else if (e.type == EventType::PttUp) {
        voice.stopRecord();
        e.durationMs = voice.recordedMs();
        sfx.stop();
        if (voice.startPlayback()) {
          sfx.setExternalBusy(true);
        }
      }
    #endif
    brain.onEvent(e, now, sfx);
    lastEvent = e;
    lastEventValid = true;
  }

  if (testAll && (now - lastTestLogAt >= 200)) {
    const Mood& m = brain.moodRef();
    const EmotionState& em = brain.emotionRef();
    const bool touch = inputs.touchActive();
    const bool ptt = inputs.pttActive();
    const float ax = imu.axG();
    const float ay = imu.ayG();
    const float az = imu.azG();
    const float roll = imu.rollDeg();
    const float pitch = imu.pitchDeg();
    const bool sfxPlay = sfx.isPlaying();
    const uint8_t sfxPrio = sfx.currentPriority();
    const int qsize = q.size();

    auto fdelta = [](float a, float b) { return fabsf(a - b); };
    const bool eventIsAction = lastEventValid && lastEvent.type != EventType::Tick1s;
    const bool stateChanged = !testPrevInit || prevState != brain.state();
    const bool emoChanged = !testPrevInit || prevEmotion != em.cur;
    const bool moodChanged = !testPrevInit ||
                             fdelta(prevMood.valence, m.valence) >= 0.02f ||
                             fdelta(prevMood.arousal, m.arousal) >= 0.02f ||
                             fdelta(prevMood.energy,  m.energy)  >= 0.02f;
    const bool inputChanged = !testPrevInit || (prevTouch != touch) || (prevPtt != ptt);
    const bool soundChanged = !testPrevInit ||
                              fdelta(prevRms, rms) >= 0.01f ||
                              fdelta(prevStrength, strength) >= 0.05f ||
                              fdelta(prevNoise, noise) >= 0.01f;
    const bool imuChanged = !testPrevInit ||
                            fdelta(prevRoll, roll) >= 2.0f ||
                            fdelta(prevPitch, pitch) >= 2.0f ||
                            fdelta(prevAx, ax) >= 0.05f ||
                            fdelta(prevAy, ay) >= 0.05f ||
                            fdelta(prevAz, az) >= 0.05f;
    const bool sfxChanged = !testPrevInit || (prevSfxPlay != sfxPlay) || (prevSfxPrio != sfxPrio);
    const bool qChanged = !testPrevInit || (prevQSize != qsize);

    const bool shouldLog =
      eventIsAction ||
      stateChanged || emoChanged || moodChanged ||
      inputChanged || soundChanged || imuChanged ||
      sfxChanged || qChanged;

    if (shouldLog && (now - lastTestLogAt >= 200)) {
      lastTestLogAt = now;
      Serial.printf(
        "TEST t=%lu st=%s emo=%s inten=%.2f mood[v=%.2f a=%.2f e=%.2f] "
        "in[touch=%d ptt=%d] snd[rms=%.3f str=%.2f nf=%.3f] "
        "imu[ax=%.2f ay=%.2f az=%.2f roll=%.1f pitch=%.1f] "
        "sfx[play=%d prio=%u] q=%d last=%s(%.2f)\n",
        (unsigned long)now,
        stateName(brain.state()),
        emoName(em.cur),
        em.intensity,
        m.valence, m.arousal, m.energy,
        touch ? 1 : 0,
        ptt ? 1 : 0,
        rms,
        strength,
        noise,
        ax, ay, az,
        roll, pitch,
        sfxPlay ? 1 : 0,
        (unsigned)sfxPrio,
        qsize,
        lastEventValid ? eventName(lastEvent.type) : "None",
        lastEventValid ? lastEvent.strength : 0.0f
      );
    }

    if (shouldLog) {
      prevState = brain.state();
      prevEmotion = em.cur;
      prevMood = m;
      prevTouch = touch;
      prevPtt = ptt;
      prevRms = rms;
      prevStrength = strength;
      prevNoise = noise;
      prevAx = ax;
      prevAy = ay;
      prevAz = az;
      prevRoll = roll;
      prevPitch = pitch;
      prevSfxPlay = sfxPlay;
      prevSfxPrio = sfxPrio;
      prevQSize = qsize;
      testPrevInit = true;
    }

  }

  if (testImu && !testAll && (now - lastImuLogAt >= 200)) {
    const float ax = imu.axG();
    const float ay = imu.ayG();
    const float az = imu.azG();
    const float roll = imu.rollDeg();
    const float pitch = imu.pitchDeg();

    auto fdelta = [](float a, float b) { return fabsf(a - b); };
    const bool imuChanged = !imuTestPrevInit ||
                            fdelta(imuPrevRoll, roll) >= 2.0f ||
                            fdelta(imuPrevPitch, pitch) >= 2.0f ||
                            fdelta(imuPrevAx, ax) >= 0.05f ||
                            fdelta(imuPrevAy, ay) >= 0.05f ||
                            fdelta(imuPrevAz, az) >= 0.05f;

    if (imuChanged) {
      lastImuLogAt = now;
      Serial.printf("TEST IMU t=%lu ax=%.2f ay=%.2f az=%.2f roll=%.1f pitch=%.1f\n",
                    (unsigned long)now, ax, ay, az, roll, pitch);
      imuPrevAx = ax;
      imuPrevAy = ay;
      imuPrevAz = az;
      imuPrevRoll = roll;
      imuPrevPitch = pitch;
      imuTestPrevInit = true;
    }
  }

  renderer.render(now, brain.state(), brain.moodRef(), brain.emotionRef(),
                  imu.rollDeg(), imu.pitchDeg(), console.isTestMode(),
                  soundDet.lastRms());

  delay(FRAME_DELAY_MS);
}
