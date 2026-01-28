#include "serial_console.h"
#include "config.h"
#include <string.h>
#include <ctype.h>

void SerialConsole::begin(Stream& s, AudioSfx& a, AudioI2S& o) {
  io = &s;
  audio = &a;
  out = &o;
  len = 0;
  PresetId loadedPreset = PresetId::Default;
  bool loaded = storage.load(a, &loadedPreset);
  if (loaded) currentPreset = loadedPreset;
  if (io) {
    io->println();
    io->println("Serial console ready. Type: help");
    if (loaded) {
      io->printf("Loaded SFX from NVS. preset=%s rate=%u\n", presetName(currentPreset), (unsigned)a.getRateLimitMs());
    } else {
      io->println("No SFX in NVS (or version mismatch). Using defaults.");
    }
  }
}

static void trim(char* s) {
  // trim leading
  while (*s && isspace((unsigned char)*s)) memmove(s, s+1, strlen(s));
  // trim trailing
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = 0;
}

bool SerialConsole::tokenEq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = tolower((unsigned char)*a++);
    char cb = tolower((unsigned char)*b++);
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

float SerialConsole::parseFloat(const char* s, bool* ok) {
  if (!s) { *ok = false; return 0; }
  char* end = nullptr;
  float v = strtof(s, &end);
  *ok = (end && end != s);
  return v;
}

long SerialConsole::parseLong(const char* s, bool* ok) {
  if (!s) { *ok = false; return 0; }
  char* end = nullptr;
  long v = strtol(s, &end, 10);
  *ok = (end && end != s);
  return v;
}

Wave SerialConsole::parseWave(const char* s, bool* ok) {
  *ok = true;
  if (!s) { *ok = false; return Wave::Sine; }
  if (tokenEq(s, "sine") || tokenEq(s, "0")) return Wave::Sine;
  if (tokenEq(s, "square") || tokenEq(s, "1")) return Wave::Square;
  if (tokenEq(s, "noise") || tokenEq(s, "2")) return Wave::Noise;
  *ok = false;
  return Wave::Sine;
}

void SerialConsole::update(uint32_t nowMs) {
  if (!io || !audio) return;

  while (io->available()) {
    int c = io->read();
    if (c < 0) break;

    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = 0;
      trim(buf);
      if (len > 0) handleLine(buf, nowMs);
      len = 0;
      continue;
    }

    if (len < sizeof(buf)-1) buf[len++] = (char)c;
  }
}

void SerialConsole::printTestHelp() const {
  if (!io) return;
  io->println("Test commands:");
  io->println("  test on           -> all logs");
  io->println("  test off          -> stop tests");
  io->println("  test mic          -> mic levels + mic warn/ok");
  io->println("  test imu          -> imu logs (ax/ay/az/roll/pitch)");
  io->println("  test sound <id|name|emotion>");
  io->println("Examples:");
  io->println("  test mic");
  io->println("  test imu");
  io->println("  test sound happy");
  io->println("  test sound 3");
}

bool SerialConsole::parseSfxId(const char* s, SfxId* outId) const {
  if (!s || !outId) return false;
  bool ok = false;
  long idv = parseLong(s, &ok);
  if (ok && idv >= 0 && idv < (long)SfxId::Count) {
    *outId = (SfxId)idv;
    return true;
  }

  if (tokenEq(s, "ready")) { *outId = SfxId::Ready; return true; }
  if (tokenEq(s, "chirpup") || tokenEq(s, "happy")) { *outId = SfxId::ChirpUp; return true; }
  if (tokenEq(s, "chirpmid") || tokenEq(s, "curious") || tokenEq(s, "neutral")) { *outId = SfxId::ChirpMid; return true; }
  if (tokenEq(s, "pop") || tokenEq(s, "surprised")) { *outId = SfxId::Pop; return true; }
  if (tokenEq(s, "sigh") || tokenEq(s, "sad") || tokenEq(s, "sleepy")) { *outId = SfxId::Sigh; return true; }
  if (tokenEq(s, "scared")) { *outId = SfxId::Scared; return true; }
  if (tokenEq(s, "down")) { *outId = SfxId::Down; return true; }
  if (tokenEq(s, "annoyed") || tokenEq(s, "angry")) { *outId = SfxId::Annoyed; return true; }
  if (tokenEq(s, "think") || tokenEq(s, "thinking")) { *outId = SfxId::Think; return true; }
  if (tokenEq(s, "confirm")) { *outId = SfxId::Confirm; return true; }
  if (tokenEq(s, "soft")) { *outId = SfxId::Soft; return true; }
  if (tokenEq(s, "wake")) { *outId = SfxId::Wake; return true; }

  return false;
}

void SerialConsole::handleLine(const char* lineIn, uint32_t nowMs) {
  if (!io || !audio) return;

  // Copy into a local buffer we can strtok
  char line[160];
  strncpy(line, lineIn, sizeof(line)-1);
  line[sizeof(line)-1] = 0;

  const char* t0 = strtok(line, " ");
  if (!t0) return;

  if (tokenEq(t0, "help")) {
    io->println("Commands:");
    io->println("  help");
    io->println("  test on|off|mic|imu|sound"); // test off
    io->println("  mic on|off|toggle");
    io->println("  vol [0..1.5|0..100]"); // vol 0.2
    io->println("  sfx list");
    io->println("  sfx dump");
    io->println("  sfx play <id> [prio 0..2]");
    io->println("  sfx set <id> f0|f1|ms|amp|wave <value>");
    io->println("  sfx rate <ms>");
    io->println("  sfx reset");
    io->println("  sfx preset <default|cute|serious|emo|stackchan>");
    io->println("  sfx show");
    io->println("  sfx save");
    io->println("  sfx load");
    io->println("  sfx clear");
    io->println("Examples:");
    io->println("  sfx play 3 2");
    io->println("  sfx set 3 amp 0.55");
    io->println("  sfx set 5 f0 1800");
    io->println("  sfx set 5 f1 900");
    io->println("  sfx dump");
    io->println("  test mic");
    io->println("  test imu");
    io->println("  test sound happy");
    return;
  }

  if (tokenEq(t0, "test")) {
    const char* tmode = strtok(nullptr, " ");
    if (!tmode) {
      const char* mode =
        (testMode == TestMode::All) ? "ALL" :
        (testMode == TestMode::Mic) ? "MIC" :
        (testMode == TestMode::Imu) ? "IMU" :
        (testMode == TestMode::Sound) ? "SOUND" : "OFF";
      io->printf("test mode: %s\n", mode);
      printTestHelp();
      return;
    }
    if (tokenEq(tmode, "on")) {
      testMode = TestMode::All;
      io->println("test mode ALL");
      printTestHelp();
    } else if (tokenEq(tmode, "off")) {
      testMode = TestMode::Off;
      io->println("test mode OFF");
    } else if (tokenEq(tmode, "mic")) {
      testMode = TestMode::Mic;
      io->println("test mode MIC");
      printTestHelp();
    } else if (tokenEq(tmode, "imu")) {
      testMode = TestMode::Imu;
      io->println("test mode IMU");
      printTestHelp();
    } else if (tokenEq(tmode, "sound")) {
      testMode = TestMode::Sound;
      printTestHelp();
      const char* tname = strtok(nullptr, " ");
      if (!tname) {
        io->println("Usage: test sound <id|name|emotion>");
        io->println("Names: ready, chirpup, chirpmid, pop, sigh, scared, down, annoyed, think, confirm, soft, wake");
        return;
      }
      SfxId id;
      if (!parseSfxId(tname, &id)) {
        io->println("Unknown sound. Use sfx list to see IDs.");
        return;
      }
      audio->playPriority(id, 1, nowMs);
      SfxDef d = audio->getDef(id);
      io->printf("SFX %d: f0=%.1f f1=%.1f ms=%u amp=%.2f wave=%u\n",
                 (int)id, d.f0, d.f1, (unsigned)d.ms, d.amp, (unsigned)d.wave);
      if (out) io->printf("volume=%.2f\n", out->getTxGain());
      io->printf("rateLimitMs=%u\n", (unsigned)audio->getRateLimitMs());
    } else {
      io->println("Usage: test on|off|mic|imu|sound");
    }
    return;
  }

  if (tokenEq(t0, "mic")) {
    const char* tmode = strtok(nullptr, " ");
    if (!tmode || tokenEq(tmode, "toggle")) {
      micMonitorEnabled = !micMonitorEnabled;
      io->printf("mic monitor: %s\n", micMonitorEnabled ? "ON" : "OFF");
      return;
    }
    if (tokenEq(tmode, "on")) {
      micMonitorEnabled = true;
      io->println("mic monitor ON");
      return;
    }
    if (tokenEq(tmode, "off")) {
      micMonitorEnabled = false;
      io->println("mic monitor OFF");
      return;
    }
    io->println("Usage: mic on|off|toggle");
    return;
  }

  if (tokenEq(t0, "vol")) {
    if (!out) { io->println("Audio output not set"); return; }
    const char* tvol = strtok(nullptr, " ");
    if (!tvol) {
      io->printf("volume=%.2f\n", out->getTxGain());
      return;
    }
    bool ok = false;
    float v = parseFloat(tvol, &ok);
    if (!ok || v < 0.0f) { io->println("Usage: vol <0..1.5|0..100>"); return; }
    if (v > 1.5f) v = v / 100.0f;
    if (v > 1.5f) v = 1.5f;
    out->setTxGain(v);
    io->printf("volume set to %.2f\n", v);
    return;
  }

  if (!tokenEq(t0, "sfx")) {
    io->println("Unknown command. Type: help");
    return;
  }

  const char* t1 = strtok(nullptr, " ");
  if (!t1) { io->println("sfx requires subcommand. Type: help"); return; }

  if (tokenEq(t1, "list")) {
    io->println("SfxId mapping:");
    io->println("  0 Ready");
    io->println("  1 ChirpUp");
    io->println("  2 ChirpMid");
    io->println("  3 Pop");
    io->println("  4 Sigh");
    io->println("  5 Scared");
    io->println("  6 Down");
    io->println("  7 Annoyed");
    io->println("  8 Think");
    io->println("  9 Confirm");
    io->println(" 10 Soft");
    io->println(" 11 Wake");
    return;
  }

  if (tokenEq(t1, "dump")) {
    audio->dumpDefs(*io);
    io->printf("rateLimitMs=%u\n", (unsigned)audio->getRateLimitMs());
    return;
  }

  if (tokenEq(t1, "reset")) {
    audio->resetDefaults();
    currentPreset = PresetId::Default;
    io->println("SFX reset to defaults.");
    return;
  }

  if (tokenEq(t1, "show")) {
    io->printf("preset=%s rateLimitMs=%u\n", presetName(currentPreset), (unsigned)audio->getRateLimitMs());
    return;
  }

  if (tokenEq(t1, "preset")) {
    const char* name = strtok(nullptr, " ");
    if (!name) { io->println("Usage: sfx preset <default|cute|serious|emo|stackchan>"); return; }
    PresetId p = PresetId::Default;
    if (!parsePreset(name, &p)) { io->println("Unknown preset. Use default|cute|serious|emo|stackchan"); return; }
    applyPreset(p, *audio);
    currentPreset = p;
    io->printf("Applied preset: %s\n", presetName(currentPreset));
    return;
  }

  if (tokenEq(t1, "save")) {
    bool ok = storage.save(*audio, currentPreset);
    io->println(ok ? "Saved SFX to NVS." : "Save failed.");
    return;
  }

  if (tokenEq(t1, "load")) {
    PresetId p = PresetId::Default;
    bool ok = storage.load(*audio, &p);
    if (ok) currentPreset = p;
    io->println(ok ? "Loaded SFX from NVS." : "Load failed (empty or version mismatch).");
    if (ok) io->printf("preset=%s rateLimitMs=%u\n", presetName(currentPreset), (unsigned)audio->getRateLimitMs());
    return;
  }

  if (tokenEq(t1, "clear")) {
    storage.clear();
    io->println("Cleared SFX NVS namespace.");
    return;
  }

  if (tokenEq(t1, "rate")) {
    const char* tms = strtok(nullptr, " ");
    bool ok=false;
    long v = parseLong(tms, &ok);
    if (!ok || v < 0) { io->println("Usage: sfx rate <ms>"); return; }
    // Rate limit is owned by AudioSfx instance; use setter
    audio->setRateLimitMs((uint32_t)v);
    io->printf("rateLimitMs set to %ld\n", v);
    return;
  }

  if (tokenEq(t1, "play")) {
    const char* tid = strtok(nullptr, " ");
    const char* tprio = strtok(nullptr, " ");
    bool ok=false;
    long idv = parseLong(tid, &ok);
    if (!ok || idv < 0 || idv >= (long)SfxId::Count) { io->println("Usage: sfx play <id 0..11> [prio 0..2]"); return; }
    uint8_t prio = 1;
    if (tprio) {
      bool ok2=false;
      long pv = parseLong(tprio, &ok2);
      if (ok2 && pv >= 0 && pv <= 2) prio = (uint8_t)pv;
    }
    audio->playPriority((SfxId)idv, prio, nowMs);
    io->printf("Played sfx %ld prio=%u\n", idv, (unsigned)prio);
    return;
  }

  if (tokenEq(t1, "set")) {
    const char* tid = strtok(nullptr, " ");
    const char* field = strtok(nullptr, " ");
    const char* val = strtok(nullptr, " ");

    bool ok=false;
    long idv = parseLong(tid, &ok);
    if (!ok || idv < 0 || idv >= (long)SfxId::Count) { io->println("Usage: sfx set <id 0..11> f0|f1|ms|amp|wave <value>"); return; }
    if (!field || !val) { io->println("Usage: sfx set <id> f0|f1|ms|amp|wave <value>"); return; }

    SfxDef d = audio->getDef((SfxId)idv);

    if (tokenEq(field, "f0")) {
      bool okf=false; float v=parseFloat(val,&okf);
      if (!okf || v < 10 || v > 8000) { io->println("f0 must be 10..8000"); return; }
      d.f0 = v;
    } else if (tokenEq(field, "f1")) {
      bool okf=false; float v=parseFloat(val,&okf);
      if (!okf || v < 10 || v > 8000) { io->println("f1 must be 10..8000"); return; }
      d.f1 = v;
    } else if (tokenEq(field, "ms")) {
      bool okm=false; long v=parseLong(val,&okm);
      if (!okm || v < 20 || v > 2000) { io->println("ms must be 20..2000"); return; }
      d.ms = (uint16_t)v;
    } else if (tokenEq(field, "amp")) {
      bool oka=false; float v=parseFloat(val,&oka);
      if (!oka || v < 0.0f || v > 1.0f) { io->println("amp must be 0..1"); return; }
      d.amp = v;
    } else if (tokenEq(field, "wave")) {
      bool okw=false; Wave w=parseWave(val,&okw);
      if (!okw) { io->println("wave must be sine|square|noise"); return; }
      d.wave = w;
    } else {
      io->println("Unknown field. Use f0|f1|ms|amp|wave");
      return;
    }

    audio->setDef((SfxId)idv, d);
    io->printf("Updated sfx %ld: f0=%.1f f1=%.1f ms=%u amp=%.2f wave=%u\n",
               idv, d.f0, d.f1, (unsigned)d.ms, d.amp, (unsigned)d.wave);
    return;
  }

  io->println("Unknown sfx subcommand. Type: help");
}
