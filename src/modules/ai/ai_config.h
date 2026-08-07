#pragma once

#include <stdint.h>

namespace axiom::ai {

#ifndef AXIOM_AI
#define AXIOM_AI 1
#endif

constexpr const char* kAiVersion = "1.0.0";
constexpr const char* kAiPrefsNs = "axiom_ai";
constexpr const char* kAiFsRoot = "/ai";
constexpr const char* kAiHistoryPath = "/ai/history.jsonl";
constexpr const char* kAiMemoryPath = "/ai/memory.json";
constexpr const char* kAiEventsPath = "/ai/events.log";

constexpr uint32_t kAiTaskPeriodMs = 50;
constexpr uint32_t kAiTaskStackWords = 16384;
constexpr uint32_t kAiTaskPriority = 1;

constexpr uint16_t kMaxChatMessages = 12;
constexpr uint16_t kMaxMessageChars = 768;
constexpr uint16_t kMaxStreamChunk = 160;
constexpr uint16_t kMaxResponseChars = 1024;
constexpr uint16_t kMaxPromptChars = 384;
constexpr uint16_t kMaxHistoryFilesLines = 64;
constexpr uint16_t kMaxMemoryFacts = 32;
constexpr uint16_t kMaxFactChars = 96;
constexpr uint16_t kMaxKnowledgeHit = 320;
constexpr uint16_t kMaxEvents = 16;
constexpr uint16_t kMaxEventChars = 72;
constexpr uint16_t kMaxActionArgs = 64;
constexpr uint8_t kTypingCharsPerTick = 4;
constexpr uint8_t kChatWrapWidth = 36;
constexpr uint8_t kChatViewMaxLines = 96;

// Optional local override (gitignored): src/modules/ai/ai_secrets.h
#if __has_include("modules/ai/ai_secrets.h")
#include "modules/ai/ai_secrets.h"
#endif

#ifndef AXIOM_AI_API_KEY
#define AXIOM_AI_API_KEY ""
#endif
#ifndef AXIOM_AI_MODEL
#define AXIOM_AI_MODEL "gpt-oss:20b"
#endif

// =============================================================================
// AI API — Ollama Cloud (https://ollama.com/v1)
// Key/model: правь ai_secrets.h (не коммить в public repo)
// =============================================================================

constexpr bool kDefaultUseHttps = true;

// REST OpenAI-compatible
constexpr const char* kDefaultRestHost = "ollama.com";
constexpr uint16_t kDefaultRestPort = 443;
constexpr const char* kDefaultRestPath = "/v1/chat/completions";

// WebSocket (local server only; cloud uses REST)
constexpr const char* kDefaultWsHost = "ollama.com";
constexpr uint16_t kDefaultWsPort = 443;
constexpr const char* kDefaultWsPath = "/v1/chat/ws";

constexpr const char* kDefaultModel = AXIOM_AI_MODEL;
constexpr const char* kDefaultApiKey = AXIOM_AI_API_KEY;

// 0 = REST, 1 = WebSocket
constexpr uint8_t kDefaultTransport = 0;
// Non-stream more reliable on ESP32+TLS; set true if you want SSE
constexpr bool kDefaultStream = false;

}  // namespace axiom::ai
