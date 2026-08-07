#include "modules/ai/AIMemory.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "services/storage/storage_service.h"

namespace axiom::ai {

bool AIMemory::Begin(services::StorageService* storage) {
  storage_ = storage;
  ready_ = EnsureFs();
  Load();
  return ready_;
}

bool AIMemory::EnsureFs() {
  if (!LittleFS.begin(true)) {
    return false;
  }
  if (!LittleFS.exists(kAiFsRoot)) {
    LittleFS.mkdir(kAiFsRoot);
  }
  return true;
}

void AIMemory::Tick() {
  if (!dirty_) return;
  const uint32_t now = millis();
  if (now - last_save_ms_ >= 5000U) {
    Save();
    last_save_ms_ = now;
  }
}

int AIMemory::FindFact(const char* key) const {
  if (!key) return -1;
  for (uint8_t i = 0; i < fact_count_; ++i) {
    if (strcmp(facts_[i].key, key) == 0) return static_cast<int>(i);
  }
  return -1;
}

bool AIMemory::SetFact(const char* key, const char* value) {
  if (!key || !value || key[0] == 0) return false;
  const int idx = FindFact(key);
  if (idx >= 0) {
    strncpy(facts_[idx].value, value, sizeof(facts_[idx].value) - 1);
    facts_[idx].value[sizeof(facts_[idx].value) - 1] = 0;
    facts_[idx].ms = millis();
    dirty_ = true;
    return true;
  }
  if (fact_count_ >= kMaxMemoryFacts) {
    memmove(&facts_[0], &facts_[1], sizeof(MemoryFact) * (kMaxMemoryFacts - 1));
    --fact_count_;
  }
  MemoryFact& f = facts_[fact_count_++];
  memset(&f, 0, sizeof(f));
  strncpy(f.key, key, sizeof(f.key) - 1);
  strncpy(f.value, value, sizeof(f.value) - 1);
  f.ms = millis();
  dirty_ = true;
  return true;
}

bool AIMemory::GetFact(const char* key, char* dst, size_t n) const {
  const int idx = FindFact(key);
  if (idx < 0 || !dst || n == 0) return false;
  strncpy(dst, facts_[idx].value, n - 1);
  dst[n - 1] = 0;
  return true;
}

void AIMemory::PushEvent(const char* text) {
  if (!text || !text[0]) return;
  if (event_count_ >= kMaxEvents) {
    memmove(&events_[0], &events_[1], sizeof(MemoryEvent) * (kMaxEvents - 1));
    --event_count_;
  }
  MemoryEvent& e = events_[event_count_++];
  memset(&e, 0, sizeof(e));
  strncpy(e.text, text, sizeof(e.text) - 1);
  e.ms = millis();
  dirty_ = true;

  if (ready_) {
    File f = LittleFS.open(kAiEventsPath, FILE_APPEND);
    if (f) {
      f.printf("%lu %s\n", static_cast<unsigned long>(e.ms), e.text);
      f.close();
    }
  }
}

bool AIMemory::AppendHistoryLine(const char* role, const char* text) {
  if (!ready_ || !role || !text) return false;
  File f = LittleFS.open(kAiHistoryPath, FILE_APPEND);
  if (!f) return false;
  f.printf("{\"ts\":%lu,\"role\":\"%s\",\"text\":\"", static_cast<unsigned long>(millis()),
           role);
  for (const char* p = text; *p; ++p) {
    if (*p == '"' || *p == '\\') f.write('\\');
    if (*p == '\n') {
      f.print("\\n");
    } else {
      f.write(static_cast<uint8_t>(*p));
    }
  }
  f.println("\"}");
  f.close();
  return true;
}

bool AIMemory::LoadHistoryTail(char* dst, size_t n, uint8_t max_lines) {
  if (!dst || n == 0) return false;
  dst[0] = 0;
  if (!ready_ || !LittleFS.exists(kAiHistoryPath)) return false;
  File f = LittleFS.open(kAiHistoryPath, FILE_READ);
  if (!f) return false;

  // Simple: read all into ring of last max_lines (store offsets by re-reading)
  // For RAM: keep last lines in a small scratch by scanning twice.
  uint32_t line_starts[kMaxHistoryFilesLines];
  uint8_t lines = 0;
  uint32_t pos = 0;
  while (f.available() && lines < kMaxHistoryFilesLines) {
    line_starts[lines++] = pos;
    while (f.available()) {
      const char c = static_cast<char>(f.read());
      ++pos;
      if (c == '\n') break;
    }
  }
  f.close();

  const uint8_t take = (lines > max_lines) ? max_lines : lines;
  const uint8_t start = static_cast<uint8_t>(lines - take);
  f = LittleFS.open(kAiHistoryPath, FILE_READ);
  if (!f) return false;
  size_t off = 0;
  for (uint8_t i = start; i < lines; ++i) {
    f.seek(line_starts[i]);
    String line = f.readStringUntil('\n');
    if (off + line.length() + 2 >= n) break;
    memcpy(dst + off, line.c_str(), line.length());
    off += line.length();
    dst[off++] = '\n';
  }
  dst[off] = 0;
  f.close();
  return off > 0;
}

void AIMemory::ClearHistory() {
  if (!ready_) return;
  LittleFS.remove(kAiHistoryPath);
}

bool AIMemory::RememberWifi(const char* ssid) {
  return SetFact("last_wifi", ssid ? ssid : "");
}

bool AIMemory::RememberRf(uint8_t channel, uint8_t power) {
  char buf[24];
  snprintf(buf, sizeof(buf), "ch=%u pwr=%u", channel, power);
  return SetFact("last_rf", buf);
}

bool AIMemory::RememberCommand(const char* cmd) {
  PushEvent(cmd);
  return SetFact("last_cmd", cmd ? cmd : "");
}

bool AIMemory::Save() {
  Preferences prefs;
  if (!prefs.begin(kAiPrefsNs, false)) return false;
  prefs.putUChar("facts", fact_count_);
  for (uint8_t i = 0; i < fact_count_; ++i) {
    char kkey[12], vkey[12];
    snprintf(kkey, sizeof(kkey), "fk%u", i);
    snprintf(vkey, sizeof(vkey), "fv%u", i);
    prefs.putString(kkey, facts_[i].key);
    prefs.putString(vkey, facts_[i].value);
  }
  prefs.end();

  if (ready_) {
    File f = LittleFS.open(kAiMemoryPath, FILE_WRITE);
    if (f) {
      f.println("{");
      for (uint8_t i = 0; i < fact_count_; ++i) {
        f.printf("  \"%s\": \"%s\"%s\n", facts_[i].key, facts_[i].value,
                 (i + 1 < fact_count_) ? "," : "");
      }
      f.println("}");
      f.close();
    }
  }
  dirty_ = false;
  return true;
}

bool AIMemory::Load() {
  Preferences prefs;
  if (!prefs.begin(kAiPrefsNs, true)) return false;
  fact_count_ = prefs.getUChar("facts", 0);
  if (fact_count_ > kMaxMemoryFacts) fact_count_ = kMaxMemoryFacts;
  for (uint8_t i = 0; i < fact_count_; ++i) {
    char kkey[12], vkey[12];
    snprintf(kkey, sizeof(kkey), "fk%u", i);
    snprintf(vkey, sizeof(vkey), "fv%u", i);
    prefs.getString(kkey, facts_[i].key, sizeof(facts_[i].key));
    prefs.getString(vkey, facts_[i].value, sizeof(facts_[i].value));
    facts_[i].ms = 0;
  }
  prefs.end();
  dirty_ = false;
  return true;
}

}  // namespace axiom::ai
