#pragma once

#include <stddef.h>
#include <stdint.h>

namespace axiom::ai {

// Voice-ready architecture (no DSP yet). Wake word target: "Axiom".

enum class VoiceEventType : uint8_t {
  None = 0,
  WakeDetected,
  PartialTranscript,
  FinalTranscript,
  TtsStarted,
  TtsFinished,
  Error
};

struct VoiceEvent {
  VoiceEventType type = VoiceEventType::None;
  char text[96] = {0};
};

class IAudioCapture {
 public:
  virtual ~IAudioCapture() = default;
  virtual bool Start(uint16_t sample_rate_hz) = 0;
  virtual void Stop() = 0;
  virtual int Read(int16_t* pcm, int max_samples) = 0;
};

class IWakeWordDetector {
 public:
  virtual ~IWakeWordDetector() = default;
  virtual bool Feed(const int16_t* pcm, int samples) = 0;
  virtual const char* WakeWord() const { return "Axiom"; }
};

class ISpeechToText {
 public:
  virtual ~ISpeechToText() = default;
  virtual bool BeginUtterance() = 0;
  virtual bool Feed(const int16_t* pcm, int samples) = 0;
  virtual bool EndUtterance(char* dst, size_t n) = 0;
};

class AIVoice {
 public:
  void SetCapture(IAudioCapture* c) { capture_ = c; }
  void SetWake(IWakeWordDetector* w) { wake_ = w; }
  void SetStt(ISpeechToText* s) { stt_ = s; }

  bool IsReady() const { return capture_ && wake_ && stt_; }
  void SetEnabled(bool v) { enabled_ = v; }
  bool Enabled() const { return enabled_; }

  void Tick();
  bool PollEvent(VoiceEvent& out);

  // Placeholder pipeline for future mic integration
  void InjectTranscript(const char* text);

 private:
  IAudioCapture* capture_ = nullptr;
  IWakeWordDetector* wake_ = nullptr;
  ISpeechToText* stt_ = nullptr;
  bool enabled_ = false;
  bool listening_ = false;
  VoiceEvent queue_[4];
  uint8_t q_head_ = 0, q_tail_ = 0, q_count_ = 0;

  void Push(VoiceEventType t, const char* text);
};

}  // namespace axiom::ai
