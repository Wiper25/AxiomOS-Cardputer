#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdint.h>

#include "modules/voice/voice_audio_io.h"
#include "modules/voice/voice_config.h"
#include "modules/voice/voice_vad.h"
#include "modules/voice/voice_ws.h"

namespace axiom {
namespace drivers {
class AudioDriver;
}
}  // namespace axiom

namespace axiom::voice {

enum class State : uint8_t { Idle = 0, Listen, Thinking, Speak };

struct VoiceTelemetry {
  State state = State::Idle;
  bool enabled = false;
  bool ws_connected = false;
  bool audio_exclusive = false;
  uint32_t vad_level = 0;
  uint32_t tx_chunks = 0;
  uint32_t rx_chunks = 0;
  char status[40] = "voice off";
};

class VoiceAssistant {
 public:
  bool Begin(const VoiceConfig& cfg = VoiceConfig{});
  void SetAudioDriver(drivers::AudioDriver* audio) { audio_ = audio; }
  void SetEnabled(bool v);
  bool Enabled() const { return enabled_; }
  void ForceListen();      // UI / non-PTT start (VAD can end)
  void PttDown();          // V pressed — start / keep listen
  void PttUp();            // V released — end after kPttHangoverMs
  void FinishUtterance();  // stop mic now → send end
  void Cancel();           // abort without ASR
  void Tick();

  State GetState() const { return state_; }
  bool IsAudioExclusive() const { return state_ != State::Idle; }
  VoiceTelemetry GetTelemetry() const;
  VoiceConfig& Config() { return cfg_; }
  const VoiceConfig& Config() const { return cfg_; }

 private:
  static void MicTaskEntry(void* arg);
  static void SpkTaskEntry(void* arg);
  void MicTask();
  void SpkTask();

  void EnterIdle();
  void EnterListen(bool from_vad);
  void EnterThinking();
  void EnterSpeak();
  void SetStatus(const char* s);
  bool EnsureWs();
  void FlushTxQueue();

  VoiceConfig cfg_;
  VoiceAudioIo audio_io_;
  EnergyVad vad_;
  VoiceWs ws_;
  drivers::AudioDriver* audio_ = nullptr;

  volatile State state_ = State::Idle;
  bool enabled_ = false;
  bool began_ = false;
  bool force_listen_ = false;
  bool pending_listen_ = false;  // V during Think/Speak → Listen after Idle
  bool cancel_req_ = false;
  bool speak_finishing_ = false;  // got server done / draining DMA
  bool got_real_pcm_ = false;     // non-silence TTS (ignore keepalive zeros)
  volatile bool wake_req_ = false;
  volatile bool ptt_held_ = false;
  uint32_t ptt_release_ms_ = 0;  // 0 = no hangover; else millis at release
  bool ptt_session_ = false;     // ignore VAD end while PTT / hangover
  uint32_t listen_started_ms_ = 0;
  uint32_t last_rx_ms_ = 0;
  uint32_t tx_chunks_ = 0;
  uint32_t rx_chunks_ = 0;
  char status_[40] = "voice off";

  QueueHandle_t tx_queue_ = nullptr;  // chunks to send (int16[kChunkSamples])
  TaskHandle_t mic_task_ = nullptr;
  TaskHandle_t spk_task_ = nullptr;
  volatile bool mic_run_ = false;
  volatile bool spk_run_ = false;
};

}  // namespace axiom::voice
