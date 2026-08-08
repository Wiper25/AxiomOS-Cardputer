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

// Playback EQ for tiny ADV speaker — clarity without tearing the cone.
// Bass rattles NS4150; hard peaks hash. Cut rumble, mild presence, soft ceil.
constexpr uint8_t kSpkVolume = 140;
constexpr uint8_t kSpkMagnification = 20;
constexpr int32_t kPlayGainNum = 1;  // unity — server already loud enough
constexpr int32_t kPlayGainDen = 1;
constexpr int16_t kChunkTargetPeak = 16000;  // per-chunk soft normalize
constexpr int16_t kSoftKnee = 15000;
constexpr int16_t kSoftCeil = 19000;
// 1-pole HPF ~250 Hz @ 16 kHz: y = a*(y + x - x1), a≈0.91 → 116/128
constexpr int32_t kHpAlphaNum = 116;
constexpr int32_t kHpAlphaDen = 128;
// Presence: mix (x - slowLP) — +~2–3 dB around speech band
constexpr int32_t kPresenceNum = 2;
constexpr int32_t kPresenceDen = 5;
constexpr int32_t kPresenceLpShift = 3;  // lp += (x-lp)>>3  (~1 kHz-ish)

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
