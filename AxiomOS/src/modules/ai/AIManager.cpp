#include "modules/ai/AIManager.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "modules/bluetooth/bluetooth_module.h"
#include "modules/mqtt/mqtt_module.h"
#include "modules/nrf24/nrf24_module.h"
#include "modules/sensors/sensors_module.h"
#include "modules/wifi/wifi_module.h"
#include "services/storage/storage_service.h"

namespace axiom::ai {

bool AIManager::Begin(const AiDeps& deps) {
  deps_ = deps;
  settings_.Begin();
  memory_.Begin(deps_.storage);
  knowledge_.Begin();
  conversation_.Clear();

  network_agent_ = new NetworkAgent(deps_.wifi);
  device_agent_ = new DeviceAgent(deps_.sensors);
  rf_agent_ = new RfAgent(deps_.nrf);
  auto_agent_ = new AutomationAgent(deps_.mqtt, deps_.wifi);

  ActionContext act;
  act.wifi = deps_.wifi;
  act.nrf = deps_.nrf;
  act.mqtt = deps_.mqtt;
  act.sensors = deps_.sensors;
  act.open_settings = deps_.open_settings;
  act.show_logs = deps_.show_logs;
  actions_.Bind(act);
  actions_.SetRequireConfirm(settings_.Data().confirm_actions);

  doctor_.Bind(deps_.wifi, deps_.nrf, deps_.sensors, deps_.mqtt);
  client_.Configure(settings_.Data());

  job_queue_ = xQueueCreate(6, sizeof(AiJob));
  if (!job_queue_) return false;

  memory_.PushEvent("AI boot");
  SetStatus(settings_.Data().enabled ? "AI ready" : "AI disabled");
  began_ = true;
  return true;
}

void AIManager::SetStatus(const char* s) {
  strncpy(status_, s ? s : "", sizeof(status_) - 1);
  status_[sizeof(status_) - 1] = 0;
}

IAgent* AIManager::AgentAt(uint8_t i) {
  switch (static_cast<AgentId>(i)) {
    case AgentId::Network:
      return network_agent_;
    case AgentId::Device:
      return device_agent_;
    case AgentId::Rf:
      return rf_agent_;
    case AgentId::Automation:
      return auto_agent_;
    case AgentId::Update:
      return &update_agent_;
    default:
      return nullptr;
  }
}

bool AIManager::QueueJob(const AiJob& job) {
  if (!job_queue_) return false;
  return xQueueSend(job_queue_, &job, 0) == pdTRUE;
}

bool AIManager::SubmitChat(const char* user_text) {
  if (!user_text || !user_text[0]) return false;
  AiJob job;
  job.type = AiJobType::Chat;
  strncpy(job.text, user_text, sizeof(job.text) - 1);
  return QueueJob(job);
}

bool AIManager::SubmitAgent(AgentId id) {
  AiJob job;
  job.type = AiJobType::RunAgent;
  job.agent = id;
  return QueueJob(job);
}

bool AIManager::SubmitDoctor() {
  AiJob job;
  job.type = AiJobType::RunDoctor;
  return QueueJob(job);
}

bool AIManager::RunAgentNow(AgentId id, AgentReport& out) {
  IAgent* a = AgentAt(static_cast<uint8_t>(id));
  if (!a) return false;
  return a->Analyze(out);
}

bool AIManager::BestRecommendation(AgentReport& out) {
  AgentReport best;
  best.severity = 0;
  bool any = false;
  for (uint8_t i = 0; i < AgentCount(); ++i) {
    AgentReport r;
    if (!RunAgentNow(static_cast<AgentId>(i), r)) continue;
    if (!any || r.severity > best.severity) {
      best = r;
      any = true;
    }
  }
  if (!any) return false;
  out = best;
  return true;
}

void AIManager::HandleChat(const char* user_text) {
  thinking_ = true;
  SetStatus("AI думает...");
  conversation_.SetState(ConversationState::Thinking);
  conversation_.Add(ChatRole::User, user_text);
  memory_.AppendHistoryLine("user", user_text);
  memory_.RememberCommand(user_text);

  // Explicit local KB: prefix "kb:" or "kb "
  bool force_kb = false;
  const char* q = user_text;
  if (strncmp(user_text, "kb:", 3) == 0) {
    force_kb = true;
    q = user_text + 3;
    while (*q == ' ') ++q;
  } else if (strncmp(user_text, "kb ", 3) == 0) {
    force_kb = true;
    q = user_text + 3;
  }

  // Intent → action offer (skip if asking cloud with ?)
  const ActionId intent = actions_.ParseIntent(user_text);
  if (intent != ActionId::None && !force_kb) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Распознано действие `%s`. Выполнить?", AIActions::Name(intent));
    conversation_.Add(ChatRole::Assistant, msg);
    actions_.Offer(intent, nullptr);
    if (actions_.HasPending()) {
      conversation_.Add(ChatRole::Assistant, "Подтверди: Enter=да, Del=нет");
    }
    memory_.AppendHistoryLine("assistant", msg);
    thinking_ = false;
    SetStatus("AI: confirm?");
    conversation_.SetState(ConversationState::Idle);
    return;
  }

  // Local KB only when forced or local_only mode
  if (force_kb || settings_.Data().local_only) {
    char kb[kMaxKnowledgeHit];
    if (knowledge_.Lookup(q, kb, sizeof(kb))) {
      conversation_.Add(ChatRole::Assistant, kb);
      conversation_.StartTypingAnimation();
      memory_.AppendHistoryLine("assistant", kb);
      thinking_ = false;
      SetStatus("AI: KB hit");
      return;
    }
    if (settings_.Data().local_only) {
      conversation_.Add(ChatRole::Assistant,
                        "Local only: KB miss. Выключи Local only для Ollama Cloud.");
      thinking_ = false;
      SetStatus("AI local miss");
      conversation_.SetState(ConversationState::Idle);
      return;
    }
  }

  if (!settings_.Data().enabled) {
    conversation_.Add(ChatRole::Assistant, "AI выключен в Settings.");
    thinking_ = false;
    conversation_.SetState(ConversationState::Idle);
    return;
  }

  // Live device context → system prompt (this is what makes answers useful)
  char system[720];
  size_t off = 0;
  int w = snprintf(system + off, sizeof(system) - off,
                   "You are AxiomOS on M5Stack Cardputer ADV (ESP32-S3). "
                   "Reply concise, practical, Russian preferred unless user writes English. "
                   "Suggest concrete Cardputer/AxiomOS actions when relevant "
                   "(wifi scan, nRF spectrum, MQTT, Doctor). "
                   "Do not invent hardware state — use DEVICE STATUS below.\n"
                   "DEVICE STATUS:\n");
  if (w > 0) off += static_cast<size_t>(w);

  if (deps_.wifi) {
    const auto wi = deps_.wifi->GetTelemetry();
    w = snprintf(system + off, sizeof(system) - off,
                 "- WiFi: %s ssid=%s rssi=%d\n", wi.connected ? "UP" : "DOWN",
                 wi.connected_ssid, static_cast<int>(wi.link_rssi));
    if (w > 0) off += static_cast<size_t>(w);
  }
  if (deps_.nrf) {
    const auto rf = deps_.nrf->GetTelemetry();
    w = snprintf(system + off, sizeof(system) - off,
                 "- nRF24: %s ch=%u act=%u%% pa=%u hot=%u\n", rf.present ? "OK" : "MISSING",
                 rf.current_channel, rf.activity_percent, rf.pa_level, rf.hottest_channel);
    if (w > 0) off += static_cast<size_t>(w);
  }
  if (deps_.sensors) {
    const auto s = deps_.sensors->GetTelemetry();
    w = snprintf(system + off, sizeof(system) - off,
                 "- Device: heap=%lu bat=%ld%% %dmV imu=%s\n",
                 static_cast<unsigned long>(s.free_heap), static_cast<long>(s.battery_percent),
                 static_cast<int>(s.battery_mv), s.imu_ok ? s.imu_name : "n/a");
    if (w > 0) off += static_cast<size_t>(w);
  }
  if (deps_.mqtt) {
    const auto m = deps_.mqtt->GetTelemetry();
    w = snprintf(system + off, sizeof(system) - off, "- MQTT: %s\n",
                 m.state == modules::MqttState::Connected ? "connected" : "down");
    if (w > 0) off += static_cast<size_t>(w);
  }
  // Memory facts
  const uint8_t fc = memory_.FactCount();
  if (fc > 0 && off + 40 < sizeof(system)) {
    w = snprintf(system + off, sizeof(system) - off, "MEMORY:");
    if (w > 0) off += static_cast<size_t>(w);
    for (uint8_t i = 0; i < fc && i < 4; ++i) {
      const auto& f = memory_.FactAt(i);
      w = snprintf(system + off, sizeof(system) - off, " %s=%s;", f.key, f.value);
      if (w > 0) off += static_cast<size_t>(w);
    }
    if (off + 2 < sizeof(system)) {
      system[off++] = '\n';
      system[off] = 0;
    }
  }

  client_.Configure(settings_.Data());
  conversation_.BeginAssistantStream();
  const bool ok =
      client_.RequestChatMessages(system, conversation_, user_text, settings_.Data().stream);
  if (!ok) {
    conversation_.SetError(client_.LastError());
    char tip[160];
    snprintf(tip, sizeof(tip), "Ollama error (%s). WiFi? key? model в ai_secrets.h?",
             client_.LastError());
    conversation_.Add(ChatRole::Assistant, tip);
    thinking_ = false;
    SetStatus("AI offline");
    return;
  }

  char chunk[kMaxStreamChunk];
  while (client_.TakeChunk(chunk, sizeof(chunk))) {
    conversation_.AppendAssistantChunk(chunk);
  }
  client_.Tick();
  while (client_.TakeChunk(chunk, sizeof(chunk))) {
    conversation_.AppendAssistantChunk(chunk);
  }

  if (conversation_.StreamingLen() == 0) {
    conversation_.AppendAssistantChunk("Пустой ответ сервера.");
  }
  conversation_.FinalizeAssistant();
  memory_.AppendHistoryLine("assistant", conversation_.At(conversation_.Count() - 1).text);
  thinking_ = false;
  SetStatus("AI idle");
}

void AIManager::ProcessJob(const AiJob& job) {
  switch (job.type) {
    case AiJobType::Chat:
      HandleChat(job.text);
      break;
    case AiJobType::RunAgent: {
      AgentReport r;
      if (RunAgentNow(job.agent, r)) {
        char buf[280];
        snprintf(buf, sizeof(buf), "%s\n%s\n→ %s", r.title, r.summary, r.recommendation);
        conversation_.Add(ChatRole::Assistant, buf);
        memory_.PushEvent(r.title);
        memory_.AppendHistoryLine("assistant", buf);
      }
      break;
    }
    case AiJobType::RunDoctor: {
      char buf[kMaxResponseChars];
      if (doctor_.DiagnoseText(buf, sizeof(buf))) {
        conversation_.Add(ChatRole::Assistant, buf);
        memory_.AppendHistoryLine("assistant", buf);
        memory_.PushEvent("doctor");
      }
      break;
    }
    case AiJobType::RunAction:
      actions_.Execute(job.action, job.text);
      break;
    default:
      break;
  }
}

void AIManager::Tick() {
  if (!began_ || !settings_.Data().enabled) {
    return;
  }

  memory_.Tick();
  client_.Tick();
  voice_.Tick();
  conversation_.TickTyping(kTypingCharsPerTick);

  VoiceEvent ve;
  while (voice_.PollEvent(ve)) {
    if (ve.type == VoiceEventType::FinalTranscript && ve.text[0]) {
      SubmitChat(ve.text);
    }
  }

  // Sync RF/WiFi facts occasionally
  static uint32_t last_fact_ms = 0;
  if (millis() - last_fact_ms > 15000U) {
    last_fact_ms = millis();
    if (deps_.wifi) {
      const auto w = deps_.wifi->GetTelemetry();
      if (w.connected) memory_.RememberWifi(w.connected_ssid);
    }
    if (deps_.nrf) {
      const auto n = deps_.nrf->GetTelemetry();
      if (n.present) memory_.RememberRf(n.current_channel, n.pa_level);
    }
  }

  AiJob job;
  if (xQueueReceive(job_queue_, &job, 0) == pdTRUE) {
    ProcessJob(job);
  }
}

}  // namespace axiom::ai
