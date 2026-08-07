#pragma once

#include <stdint.h>

#ifndef AXIOM_VOICE
#define AXIOM_VOICE 1
#endif

namespace axiom::voice {

constexpr uint32_t kSampleRateHz = 16000;
constexpr uint16_t kChunkSamples = 512;           // 1024 bytes int16
constexpr uint16_t kChunkBytes = kChunkSamples * sizeof(int16_t);
constexpr uint8_t kTxQueueDepth = 8;
constexpr uint8_t kRxQueueDepth = 8;

constexpr uint32_t kMicTaskStackWords = 4096;
constexpr uint32_t kSpkTaskStackWords = 4096;
constexpr uint32_t kMicTaskPriority = 2;
constexpr uint32_t kSpkTaskPriority = 2;

// Energy VAD (mean-abs)
constexpr uint32_t kVadStartThreshold = 450;      // start speech
constexpr uint32_t kVadEndThreshold = 280;        // end speech
constexpr uint8_t kVadStartFrames = 3;            // consecutive loud
constexpr uint8_t kVadEndFrames = 12;             // ~384ms quiet @ 32ms/chunk → bump via silence ms
constexpr uint32_t kSilenceEndMs = 800;
constexpr uint32_t kMaxListenMs = 12000;
constexpr uint32_t kPttHangoverMs = 2000;         // keep recording after V release
constexpr uint32_t kSpeakIdleTimeoutMs = 4000;    // no RX / drain done

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
