#pragma once

#include <stddef.h>
#include <stdint.h>

#include "modules/ai/ai_config.h"

namespace axiom::services {
class StorageService;
}

namespace axiom::ai {

struct MemoryFact {
  char key[24] = {0};
  char value[kMaxFactChars] = {0};
  uint32_t ms = 0;
};

struct MemoryEvent {
  char text[kMaxEventChars] = {0};
  uint32_t ms = 0;
};

class AIMemory {
 public:
  bool Begin(services::StorageService* storage);
  void Tick();

  bool SetFact(const char* key, const char* value);
  bool GetFact(const char* key, char* dst, size_t n) const;
  uint8_t FactCount() const { return fact_count_; }
  const MemoryFact& FactAt(uint8_t i) const { return facts_[i]; }

  void PushEvent(const char* text);
  uint8_t EventCount() const { return event_count_; }
  const MemoryEvent& EventAt(uint8_t i) const { return events_[i]; }

  bool AppendHistoryLine(const char* role, const char* text);
  bool LoadHistoryTail(char* dst, size_t n, uint8_t max_lines);
  void ClearHistory();

  bool RememberWifi(const char* ssid);
  bool RememberRf(uint8_t channel, uint8_t power);
  bool RememberCommand(const char* cmd);

  bool Save();
  bool Load();

 private:
  bool EnsureFs();
  int FindFact(const char* key) const;

  services::StorageService* storage_ = nullptr;
  MemoryFact facts_[kMaxMemoryFacts];
  uint8_t fact_count_ = 0;
  MemoryEvent events_[kMaxEvents];
  uint8_t event_count_ = 0;
  bool ready_ = false;
  bool dirty_ = false;
  uint32_t last_save_ms_ = 0;
};

}  // namespace axiom::ai
