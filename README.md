# EMO DeskBot - ESP32-S3 (PlatformIO)

Minimal first-boot project focused on behavior, emotions, sounds, and a simple OLED face.

## Hardware
- ESP32-S3 Super Mini
- OLED 1.3" 128x64 SH1106 I2C
- MAX98357A I2S amp + speaker
- PTT button
- TTP223 touch sensor

## Wiring (as configured)
I2C:
- GPIO8 SDA
- GPIO9 SCL

I2S (amp):
- GPIO3 BCLK
- GPIO4 LRCK/WS
- GPIO5 DOUT -> MAX98357A DIN

Inputs:
- GPIO7 PTT (default config assumes active-low with pullup)
- GPIO10 Touch (TTP223 HIGH on touch)

## First run
1. Open folder in VS Code.
2. Install PlatformIO extension.
3. Select environment "esp32-s3".
4. Build and Upload.
5. Open Serial Monitor at 115200.

Expected:
- OLED shows face with state label (BOOT -> IDLE).
- Touch short: happy expression + chirp.
- Touch long: sleepy + sigh when energy is low; otherwise happy + soft chirp.
- PTT hold: listening face + ready beep. Release (>250ms): thinking then speaking placeholder. Short release: back to idle.
- Touch during speaking: interrupts sound + annoyed beep.

## Configuration
See include/config.h for pins and options like PTT_ACTIVE_LOW.

## Next steps
- Add real STT/LLM/TTS pipeline (currently simulated).
- Optional: replace geometry face with bitmap-based face assets.


## INMP441 mic
- Uses I2S RX on GPIO6 (PIN_I2S_DIN).
- Clap or loud sound should trigger SURPRISED + POP.
- If too sensitive, increase threshold in src/main.cpp:
  soundDet.setPeakThreshold(0.45f);
- If not sensitive enough, reduce to ~0.25f.


## BMI270 IMU
- Uses I2C on GPIO8/9.
- On startup Serial prints "BMI270 init: OK" if detected.
- Shake: movement triggers SCARED + SFX (via existing FSM/Emotion rules).
- Tilt: tilt beyond threshold triggers ATTENTION/curious behavior.
- If init fails: check wiring and I2C address (0x68/0x69).


## Gaze-from-IMU
- Eyes follow tilt: roll/pitch from BMI270 are mapped to pupil movement.
- If gaze is too strong/weak: adjust maxDeg and pixel scale in src/render_oled.cpp.
- If jittery: increase accel low-pass (imu.setAccelLowpass(0.25f)) or reduce pupil scale.


## Startle + Attention Hold
- Startle triggers when emotion becomes SURPRISED or SCARED (clap or strong shake). Eyes open wide, pupils kick, short freeze.
- Attention hold triggers when entering ATTENTION (often from Tilt). Robot keeps gaze for ~1.5s before following IMU again.
- Tunables in src/render_oled.cpp: maxDeg, pixel scales, alpha, hold duration, startle durations.


## Breathing idle + Speech-aware gaze
- Breathing: eyebrows/mouth shift by ~1px with a slow sinusoid in Idle/Attention/Listening.
- Speech-aware gaze: after Speaking ends, gaze centers for ~0.9s (eye contact).
- Idle micro SFX: occasional ChirpMid/Sigh (rare) while calm; adjust in src/fsm.cpp.


## Sound motifs 
- Priorities:
  - high: Surprised/Scared (preempt current audio)
  - normal: UI state cues (Ready/Think/Confirm/Wake)
  - low: idle micro cues (Soft/ChirpMid)
- Non-blocking SFX: audio is streamed in the main loop; high-priority sounds can interrupt an active one.
- If sounds are too frequent: increase AUDIO_SFX_RATE_LIMIT_MS in include/config.h or reduce idle sfx chance in src/fsm.cpp.


## Serial sound tuning 
Open Serial Monitor at 115200. Set line ending to "Newline" (or "Both NL & CR").

Type `help` to see commands.

### Quick commands
- `sfx list` - list SfxId mapping
- `sfx play <id> [prio]` - play a sound now (prio: 0 low, 1 normal, 2 high)
- `sfx set <id> f0 <Hz>` - set start frequency
- `sfx set <id> f1 <Hz>` - set end frequency
- `sfx set <id> ms <duration>` - set duration in ms
- `sfx set <id> amp <0..1>` - set amplitude
- `sfx set <id> wave <sine|square|noise>` - set waveform
- `sfx rate <ms>` - change global rate-limit (ms)
- `sfx dump` - dump current defs
- `sfx reset` - restore defaults

### Example workflow (tune Pop)
1) `sfx play 3 2`
2) `sfx set 3 amp 0.55`
3) `sfx set 3 f0 1500`
4) `sfx set 3 f1 600`
5) Repeat `sfx play 3 2` until it feels right.
