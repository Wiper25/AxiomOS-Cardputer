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

// Playback (ADV ES8311+NS4150): quieter + no hard clip → clearer speech
// Server already normalizes ~peak 28k — do NOT ×3 on device (that shredded the amp).
constexpr uint8_t kSpkVolume = 168;       // was 255
constexpr uint8_t kSpkMagnification = 28; // was 64
constexpr int32_t kPlayGainNum = 5;       // digital gain = 5/4 = 1.25x mild
constexpr int32_t kPlayGainDen = 4;
constexpr int16_t kSoftKnee = 22000;      // soft-limit above this
constexpr int16_t kSoftCeil = 30000;      // never hit full-scale rail

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
