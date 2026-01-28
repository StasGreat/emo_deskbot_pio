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
- Touch long: sleepy expression + sigh.
- PTT hold: listening face + ready beep. Release: thinking then speaking placeholder.
- Touch during speaking: interrupts sound + annoyed beep.

## Configuration
See include/config.h for pins and options like PTT_ACTIVE_LOW.

## Next steps
- Replace mock sensors with real INMP441 sound peak detector.
- Add BMI270 shake/tilt detector.
- Replace geometry face with bitmap-based face assets if desired.


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
