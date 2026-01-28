# DeskBot Functional Test Checklist

Use this checklist to verify the end-to-end behavior on real hardware.

## 0) Pre-flight
- Power: USB + stable 5V to the amp.
- Wiring: confirm pins match `include/config.h`.
- Serial Monitor: 115200 baud, Newline (or Both NL&CR).

## 1) Boot / baseline
- Build and upload cleanly.
- Serial shows boot banner and pin list.
- Serial prints `BMI270 init: OK` (if not: check I2C wiring/address).
- OLED shows face + state label. BOOT -> IDLE within ~1s.
- Startup SFX plays (Ready beep).
  - Enable telemetry: type `test on` in Serial Monitor.

## 2) Inputs
- Touch short:
  - Emotion: Happy
  - SFX: ChirpUp
- Touch long:
  - If energy low: Sleepy + Sigh
  - Else: Happy + Soft chirp
- PTT down:
  - State: Listening
  - SFX: Ready
- PTT up (held >250ms):
  - State: Processing -> Speaking
  - SFX: Think then Confirm
- PTT up (short hold):
  - Returns to Idle (no think/speak)

## 3) Sound (INMP441)
- Clap / loud sound:
  - Event: SoundPeak
  - Emotion: Surprised
  - SFX: Pop (high priority, should preempt)
- If too sensitive: raise `soundDet.setPeakThreshold(...)` in `src/main.cpp`.
- If not sensitive: lower threshold.

## 4) IMU (BMI270)
- Tilt the board:
  - Pupils should shift with roll/pitch (subtle but visible).
  - Larger tilt triggers ATTENTION (state label changes).
- Shake the board:
  - Emotion: Scared
  - SFX: Scared (high priority)
- If no reaction:
  - Verify `BMI270 init: OK` on Serial.
  - Check address 0x68/0x69 and I2C wiring.
  - Temporarily reduce `imu.setTiltThresholdDeg(...)` and `imu.setShakeThresholdG(...)` in `src/main.cpp`.

## 5) Mood vs Emotion (behavior sanity)
- Mood changes only on events and relaxes slowly (1s tick).
- Emotion is short-lived and overrides expression.
- Expect visible changes after events:
  - Anger: squint + brow tilt
  - Surprise: wide eyes + pop
  - Sleepy: droopy eyes + sigh

## 6) SFX preempt (non-blocking)
- While a low/normal SFX plays, trigger a high-priority event (SoundPeak or Shake).
- High-priority SFX should interrupt the current one.

## 7) Test mode telemetry (Serial)
- Command: `test on` / `test off`
- Output prefix: `TEST`
- Fields include: state, emotion, mood, touch/PTT, sound RMS/strength/noise floor,
  IMU accel/roll/pitch, SFX play/priority, queue depth, last event.
