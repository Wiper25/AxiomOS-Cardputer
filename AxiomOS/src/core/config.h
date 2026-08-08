#pragma once

#include <stdint.h>

// Voice-only build (plan: branch minimal)
#ifndef AXIOM_AI
#define AXIOM_AI 0
#endif

#ifndef AXIOM_VOICE
#define AXIOM_VOICE 1
#endif

// --- Edit these (or -DAXIOM_WIFI_SSID=... in platformio.ini) ---
#ifndef AXIOM_WIFI_SSID
#define AXIOM_WIFI_SSID "YOUR_SSID"
#endif
#ifndef AXIOM_WIFI_PASS
#define AXIOM_WIFI_PASS "YOUR_PASSWORD"
#endif
#ifndef AXIOM_VOICE_HOST
#define AXIOM_VOICE_HOST "170.168.91.194"
#endif
#ifndef AXIOM_VOICE_PORT
#define AXIOM_VOICE_PORT 8090
#endif
#ifndef AXIOM_VOICE_PATH
#define AXIOM_VOICE_PATH "/voice"
#endif

namespace axiom {

constexpr const char* kProjectName = "AxiomOS Voice";
constexpr const char* kProjectVersion = "0.2.0-minimal";

constexpr uint32_t kStatusRedrawMs = 200;

}  // namespace axiom
