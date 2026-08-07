#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "modules/ai/AIActions.h"
#include "modules/ai/AIClient.h"
#include "modules/ai/AIConversation.h"
#include "modules/ai/AIDoctor.h"
#include "modules/ai/AIKnowledge.h"
#include "modules/ai/AIMemory.h"
#include "modules/ai/AISettings.h"
#include "modules/ai/AIVoice.h"
#include "modules/ai/agents/AutomationAgent.h"
#include "modules/ai/agents/DeviceAgent.h"
#include "modules/ai/agents/NetworkAgent.h"
#include "modules/ai/agents/RfAgent.h"
#include "modules/ai/agents/UpdateAgent.h"

namespace axiom {
namespace modules {
class WifiModule;
class Nrf24Module;
class MqttModule;
class SensorsModule;
class BluetoothModule;
}  // namespace modules
namespace services {
class StorageService;
}
}  // namespace axiom

namespace axiom::ai {

enum class AiJobType : uint8_t {
  None = 0,
  Chat,
  RunAgent,
  RunDoctor,
  RunAction
};

struct AiJob {
  AiJobType type = AiJobType::None;
  AgentId agent = AgentId::Network;
  ActionId action = ActionId::None;
  char text[kMaxPromptChars] = {0};
};

struct AiDeps {
  modules::WifiModule* wifi = nullptr;
  modules::Nrf24Module* nrf = nullptr;
  modules::MqttModule* mqtt = nullptr;
  modules::SensorsModule* sensors = nullptr;
  modules::BluetoothModule* bt = nullptr;
  services::StorageService* storage = nullptr;
  void (*open_settings)() = nullptr;
  void (*show_logs)() = nullptr;
};

class AIManager {
 public:
  bool Begin(const AiDeps& deps);
  void Tick();  // call from AI FreeRTOS task

  bool Enabled() const { return settings_.Data().enabled; }
  void SetEnabled(bool v) { settings_.SetEnabled(v); }

  AISettings& Settings() { return settings_; }
  AIConversation& Conversation() { return conversation_; }
  AIMemory& Memory() { return memory_; }
  AIKnowledge& Knowledge() { return knowledge_; }
  AIActions& Actions() { return actions_; }
  AIDoctor& Doctor() { return doctor_; }
  AIVoice& Voice() { return voice_; }
  AIClient& Client() { return client_; }

  bool SubmitChat(const char* user_text);
  bool SubmitAgent(AgentId id);
  bool SubmitDoctor();
  bool QueueJob(const AiJob& job);

  bool RunAgentNow(AgentId id, AgentReport& out);
  bool BestRecommendation(AgentReport& out);

  uint8_t AgentCount() const { return static_cast<uint8_t>(AgentId::Count); }
  IAgent* AgentAt(uint8_t i);

  const char* LastStatus() const { return status_; }
  bool Thinking() const { return thinking_; }

 private:
  void ProcessJob(const AiJob& job);
  void HandleChat(const char* user_text);
  void SetStatus(const char* s);

  AiDeps deps_;
  AISettings settings_;
  AIMemory memory_;
  AIKnowledge knowledge_;
  AIConversation conversation_;
  AIClient client_;
  AIActions actions_;
  AIDoctor doctor_;
  AIVoice voice_;

  NetworkAgent* network_agent_ = nullptr;
  DeviceAgent* device_agent_ = nullptr;
  RfAgent* rf_agent_ = nullptr;
  AutomationAgent* auto_agent_ = nullptr;
  UpdateAgent update_agent_;

  QueueHandle_t job_queue_ = nullptr;
  char status_[40] = "AI idle";
  bool thinking_ = false;
  bool began_ = false;
};

}  // namespace axiom::ai
