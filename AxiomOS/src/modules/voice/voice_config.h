#pragma once

#include <stdint.h>

#ifndef AXIOM_VOICE
#define AXIOM_VOICE 1
#endif

namespace axiom::voice {

constexpr uint32_t kSampleRateHz = 16000;
constexpr uint16_t kChunkSamples = 512;
constexpr uint16_t kChunkBytes = kChunkSamples * sizeof(int16_t);
constexpr uint8_t kTxQueueDepth = 8;
constexpr uint8_t kRxQueueDepth = 24;

constexpr uint32_t kMicTaskStackWords = 4096;
constexpr uint32_t kSpkTaskStackWords = 4096;
constexpr uint32_t kMicTaskPriority = 1;
constexpr uint32_t kSpkTaskPriority = 2;

constexpr uint32_t kVadStartThreshold = 450;
constexpr uint32_t kVadEndThreshold = 280;
constexpr uint8_t kVadStartFrames = 3;
constexpr uint8_t kVadEndFrames = 12;
constexpr uint32_t kSilenceEndMs = 800;
constexpr uint32_t kMaxListenMs = 12000;
constexpr uint32_t kPttHangoverMs = 2000;
constexpr uint32_t kSpeakIdleTimeoutMs = 45000;
constexpr uint32_t kSpeakQuietExitMs = 12000;
constexpr uint8_t kListenTxBudget = 1;

constexpr const char* kDefaultWsHost = "170.168.91.194";
constexpr uint16_t kDefaultWsPort = 8090;
constexpr const char* kDefaultWsPath = "/voice";

struct VoiceConfig {
  char host[48] = {0};
  uint16_t port = kDefaultWsPort;
  char path[40] = {0};
  bool enabled = true;
};

}  // namespace axiom::voice
