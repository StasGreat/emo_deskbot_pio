# Desk Robot Roadmap - Behavior, Emotions, Sounds

This document consolidates the architecture, tables, and design decisions for the desk robot project.

## 1. System goals
- Always "alive" even without STT/LLM (idle micro-behavior).
- Fast reactions to touch, motion, and sound (200-500 ms).
- Emotions are short-lived; mood is long-lived.
- Sounds (SFX) amplify emotions and timing.
- Behavior is deterministic and testable (FSM + event queue).

## 2. Layered architecture
Sensors -> Perception -> Emotion Engine -> Behavior FSM -> Expression Layer

- Sensors: Touch, PTT, Mic energy, IMU shake/tilt.
- Perception: converts raw readings to events.
- Emotion Engine: selects emotion overlay and updates mood.
- Behavior FSM: chooses activity state (idle/listen/think/speak/sleep).
- Expression: OLED face + SFX + (future) movement.

## 3. Hardware mapping (current)
- I2C: SDA GPIO8, SCL GPIO9 (OLED SH1106 + BMI270)
- I2S: BCLK GPIO3, LRCK GPIO4, DOUT GPIO5 (MAX98357A), DIN GPIO6 (INMP441 later)
- Inputs: PTT GPIO7, Touch GPIO10

## 4. State machine (FSM)
States:
- BOOT
- IDLE
- ATTENTION
- LISTENING (hold-to-talk)
- PROCESSING
- SPEAKING
- SLEEP
- ERROR

Key rules:
- PTT is hold-to-talk (pressed = LISTENING, release -> PROCESSING or IDLE).
- Touch during SPEAKING interrupts speech and transitions to ATTENTION.

### 4.1 FSM transition table (high level)
- BOOT -> IDLE after ~1s
- IDLE + PTT_DOWN -> LISTENING
- IDLE + touch/sound/shake/tilt -> ATTENTION
- ATTENTION -> IDLE after timer
- LISTENING + PTT_UP -> PROCESSING if captured_audio else IDLE
- PROCESSING -> SPEAKING after response
- SPEAKING + touch -> interrupt -> ATTENTION
- IDLE energy low -> SLEEP
- SLEEP + touch/sound/shake/PTT -> ATTENTION/LISTENING

## 5. Mood vs Emotion
Mood (slow):
- valence (-1..+1)
- arousal (0..1)
- energy (0..1)

Emotion (fast overlay):
- Neutral, Curious, Happy, Sad, Angry, Surprised, Scared, Sleepy, Listening, Thinking

Emotion rules:
- priority-based overrides
- ttl expiration
- lock time prevents rapid switching

Priority order (high -> low):
- Scared
- Surprised
- Angry
- Listening
- Thinking
- Happy
- Curious
- Sad
- Sleepy
- Neutral

## 6. Event model
Events:
- TouchDown / TouchUp / TouchLong
- PttDown / PttUp
- SoundPeak(strength)
- Shake(strength)
- Tilt
- Tick1s

Each event has timestamp and optional strength/duration.

## 7. Event -> Mood deltas
- Touch short: valence +0.06, arousal +0.04
- Touch long: valence +0.04, energy +0.02
- SoundPeak: arousal +0.05*strength, if strong then valence -0.02
- Shake: arousal +0.12*strength, valence -0.08*strength, energy -0.02
- Relax each second: arousal *= 0.98, energy drift (up in sleep, down otherwise)

## 8. Event -> Emotion -> SFX mapping
- Touch short: Happy + ChirpUp
- Touch long: Sleepy + Sigh (if energy low), else Happy
- SoundPeak strong: Surprised + Pop
- Shake strong: Scared + Scared SFX
- PTT down: Listening + Ready beep
- Touch during speaking: Angry/Curious + Annoyed

## 9. SFX strategy
- No audio files for first version.
- Procedural SFX: chirp, pop, sigh, scared, down, annoyed.
- Generated on the fly (PCM int16) and played via I2S to MAX98357A.
- Rate limit: default 700 ms between SFX to avoid spam.

## 10. Expression layer (OLED)
- Base face depends on FSM state.
- Emotion overlay modifies eyebrows, eye openness, mouth shape.
- Idle micro behavior: blink + saccades, plus occasional micro actions (future).

## 11. Planned next integrations
Phase 1 (current):
- OLED face + procedural SFX + touch + PTT.

Phase 2:
- INMP441 energy detector -> SoundPeak events.
- BMI270 shake/tilt thresholds -> Shake/Tilt events.

Phase 3:
- STT + LLM + TTS pipeline (external engine).
- Speaking mouth synced to audio energy.

Phase 4:
- More complex behavior scheduler, personality, long-term preferences.


## 12. INMP441 integration (added)
- I2S configured as full duplex (TX+RX) on I2S0 (shared BCLK/LRCK).
- Sample format: 32-bit stereo.
- SoundDetector computes RMS on chunks and adapts noise floor.
- Emits SoundPeak events with normalized strength (0..1).
- Suppresses peaks while Listening/Speaking to reduce speaker bleed.
- Tuning: peakThreshold, noiseAdapt, minIntervalMs.
