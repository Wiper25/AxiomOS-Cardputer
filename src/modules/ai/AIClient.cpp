#include "modules/ai/AIClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <stdio.h>
#include <string.h>

#include <WebSocketsClient.h>

#include "modules/ai/AIConversation.h"

namespace axiom::ai {
namespace {

WebSocketsClient g_ws;
AIClient* g_ws_owner = nullptr;

void TruncateForPrompt(const char* src, char* dst, size_t n) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

void AppendJsonEscaped(String& out, const char* s) {
  if (!s) return;
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') out += '\\';
    if (*p == '\n')
      out += "\\n";
    else if (*p == '\r')
      continue;
    else if (*p == '\t')
      out += ' ';
    else
      out += *p;
  }
}

void WsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (g_ws_owner == nullptr) return;
  if (type == WStype_TEXT && payload && length) {
    char tmp[kMaxStreamChunk];
    const size_t n = length < sizeof(tmp) - 1 ? length : sizeof(tmp) - 1;
    memcpy(tmp, payload, n);
    tmp[n] = 0;
    g_ws_owner->OnWsPayload(tmp);
  }
}

}  // namespace

void AIClient::Configure(const AiSettingsData& settings) { cfg_ = settings; }

void AIClient::OnWsPayload(const char* text) {
  if (!text) return;
  if (strstr(text, "\"content\"")) {
    ExtractDeltaContent(text);
  } else if (strncmp(text, "[DONE]", 6) == 0) {
    state_ = ClientState::Done;
  } else {
    PushChunk(text);
  }
}

void AIClient::SetError(const char* msg) {
  state_ = ClientState::Error;
  strncpy(error_, msg ? msg : "error", sizeof(error_) - 1);
  error_[sizeof(error_) - 1] = 0;
}

void AIClient::PushChunk(const char* s) {
  if (!s || !s[0]) return;
  if (chunk_count_ >= kChunkQSize) {
    chunk_head_ = static_cast<uint8_t>((chunk_head_ + 1) % kChunkQSize);
    --chunk_count_;
  }
  strncpy(chunk_q_[chunk_tail_], s, kMaxStreamChunk - 1);
  chunk_q_[chunk_tail_][kMaxStreamChunk - 1] = 0;
  chunk_tail_ = static_cast<uint8_t>((chunk_tail_ + 1) % kChunkQSize);
  ++chunk_count_;
}

void AIClient::PushTextAsChunks(const char* text) {
  if (!text) return;
  const size_t total = strlen(text);
  for (size_t i = 0; i < total;) {
    char piece[kMaxStreamChunk];
    size_t n = 0;
    while (i < total && n + 1 < sizeof(piece)) {
      piece[n++] = text[i++];
    }
    piece[n] = 0;
    PushChunk(piece);
  }
}

bool AIClient::TakeChunk(char* dst, size_t n) {
  if (chunk_count_ == 0 || !dst || n == 0) return false;
  strncpy(dst, chunk_q_[chunk_head_], n - 1);
  dst[n - 1] = 0;
  chunk_head_ = static_cast<uint8_t>((chunk_head_ + 1) % kChunkQSize);
  --chunk_count_;
  return true;
}

bool AIClient::ExtractJsonContentString(const char* json, char* dst, size_t dst_n) {
  if (!json || !dst || dst_n == 0) return false;
  dst[0] = 0;
  const char* msg = strstr(json, "\"message\"");
  const char* key = msg ? strstr(msg, "\"content\"") : nullptr;
  if (!key) key = strstr(json, "\"content\"");
  if (!key) return false;
  const char* colon = strchr(key, ':');
  if (!colon) return false;
  while (*colon == ':' || *colon == ' ' || *colon == '\n' || *colon == '\r' || *colon == '\t') {
    ++colon;
  }
  if (*colon != '"') return false;
  ++colon;
  size_t o = 0;
  for (const char* p = colon; *p && o + 1 < dst_n; ++p) {
    if (*p == '"') break;
    if (*p == '\\' && p[1]) {
      ++p;
      if (*p == 'n')
        dst[o++] = '\n';
      else if (*p == 'r')
        continue;
      else if (*p == 't')
        dst[o++] = ' ';
      else if (*p == '"')
        dst[o++] = '"';
      else if (*p == '\\')
        dst[o++] = '\\';
      else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
        p += 4;
        dst[o++] = '?';
      } else {
        dst[o++] = *p;
      }
    } else {
      dst[o++] = *p;
    }
  }
  dst[o] = 0;
  return o > 0;
}

void AIClient::ExtractDeltaContent(const char* json_line) {
  if (!json_line) return;
  // Stream deltas are small; full non-stream uses ExtractJsonContentString.
  char out[kMaxStreamChunk];
  if (!ExtractJsonContentString(json_line, out, sizeof(out))) return;
  PushChunk(out);
}

void AIClient::ParseSseOrJsonChunk(const char* data, size_t len) {
  if (!data || len == 0) return;
  char line[256];
  size_t li = 0;
  for (size_t i = 0; i <= len; ++i) {
    const char c = (i < len) ? data[i] : '\n';
    if (c == '\n' || c == '\r' || i == len) {
      if (li == 0) continue;
      line[li] = 0;
      li = 0;
      const char* p = line;
      if (strncmp(p, "data:", 5) == 0) {
        p += 5;
        while (*p == ' ') ++p;
        if (strcmp(p, "[DONE]") == 0) {
          state_ = ClientState::Done;
          continue;
        }
        ExtractDeltaContent(p);
      } else if (strstr(p, "\"content\"")) {
        ExtractDeltaContent(p);
      }
    } else if (li + 1 < sizeof(line)) {
      line[li++] = c;
    }
  }
}

void AIClient::Abort() {
  if (ws_active_) {
    g_ws.disconnect();
    ws_active_ = false;
    g_ws_owner = nullptr;
  }
  state_ = ClientState::Idle;
  chunk_count_ = 0;
  chunk_head_ = chunk_tail_ = 0;
}

bool AIClient::RequestChat(const char* prompt, bool stream) {
  if (!WiFi.isConnected()) {
    SetError("WiFi offline");
    return false;
  }
  if (IsBusy()) {
    SetError("busy");
    return false;
  }
  error_[0] = 0;
  chunk_count_ = 0;
  chunk_head_ = chunk_tail_ = 0;
  request_started_ms_ = millis();
  state_ = ClientState::Sending;

  if (cfg_.transport == AiTransport::WebSocket) {
    return DoWebSocket(prompt, stream);
  }
  return DoRest(prompt, stream);
}

bool AIClient::RequestChatMessages(const char* system, const AIConversation& conv,
                                   const char* user_text, bool stream) {
  if (!WiFi.isConnected()) {
    SetError("WiFi offline");
    return false;
  }
  if (IsBusy()) {
    SetError("busy");
    return false;
  }
  error_[0] = 0;
  chunk_count_ = 0;
  chunk_head_ = chunk_tail_ = 0;
  request_started_ms_ = millis();
  state_ = ClientState::Sending;

  if (cfg_.transport == AiTransport::WebSocket) {
    // WS path: flatten
    char flat[768];
    snprintf(flat, sizeof(flat), "%s\nUser: %s", system ? system : "", user_text ? user_text : "");
    return DoWebSocket(flat, stream);
  }
  return DoRestMessages(system, conv, user_text, stream);
}

bool AIClient::DoRestMessages(const char* system, const AIConversation& conv,
                              const char* user_text, bool stream) {
  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  bool began = false;

  if (cfg_.use_https) {
    secure.setInsecure();
    secure.setTimeout(25000);
    began = http.begin(secure, cfg_.rest_host, cfg_.rest_port, cfg_.rest_path, true);
  } else {
    began = http.begin(plain, cfg_.rest_host, cfg_.rest_port, cfg_.rest_path, false);
  }
  if (!began) {
    SetError("http begin");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  if (cfg_.api_key[0]) {
    char auth[96];
    snprintf(auth, sizeof(auth), "Bearer %s", cfg_.api_key);
    http.addHeader("Authorization", auth);
  }
  http.setTimeout(45000);
  http.setConnectTimeout(15000);

  String body;
  body.reserve(1800);
  body += "{\"model\":\"";
  body += cfg_.model;
  body += "\",\"stream\":";
  body += stream ? "true" : "false";
  body += ",\"messages\":[";

  bool first = true;
  auto addMsg = [&](const char* role, const char* content) {
    if (!content || !content[0]) return;
    if (!first) body += ',';
    first = false;
    body += "{\"role\":\"";
    body += role;
    body += "\",\"content\":\"";
    AppendJsonEscaped(body, content);
    body += "\"}";
  };

  addMsg("system", system);

  // Prior turns only (skip last user — passed as user_text)
  const uint16_t n = conv.Count();
  uint16_t end = n;
  if (end > 0 && conv.At(end - 1).role == ChatRole::User) {
    --end;  // exclude just-added current user
  }
  const uint16_t start = (end > 6) ? static_cast<uint16_t>(end - 6) : 0;
  for (uint16_t i = start; i < end; ++i) {
    const auto& m = conv.At(i);
    const char* role = m.role == ChatRole::Assistant ? "assistant"
                       : m.role == ChatRole::System    ? "system"
                                                       : "user";
    // Truncate very long history lines for RAM/JSON size
    char clip[220];
    TruncateForPrompt(m.text, clip, sizeof(clip));
    addMsg(role, clip);
  }
  addMsg("user", user_text);
  body += "]}";

  state_ = ClientState::Streaming;
  const int code = http.POST(body);
  if (code < 200 || code >= 300) {
    char err[48];
    snprintf(err, sizeof(err), (code == 401 || code == 403) ? "HTTP %d auth" : "HTTP %d", code);
    http.end();
    SetError(err);
    return false;
  }

  if (stream) {
    WiFiClient* sock = http.getStreamPtr();
    if (!sock) {
      http.end();
      SetError("no stream");
      return false;
    }
    const uint32_t t0 = millis();
    while (http.connected() && (millis() - t0 < 45000U)) {
      while (sock->available()) {
        String line = sock->readStringUntil('\n');
        ParseSseOrJsonChunk(line.c_str(), line.length());
        if (state_ == ClientState::Done) break;
      }
      if (state_ == ClientState::Done) break;
      delay(1);
    }
    if (state_ != ClientState::Done && state_ != ClientState::Error) state_ = ClientState::Done;
  } else {
    String resp = http.getString();
    char full[kMaxResponseChars];
    if (ExtractJsonContentString(resp.c_str(), full, sizeof(full))) {
      PushTextAsChunks(full);
    } else {
      char preview[kMaxStreamChunk];
      strncpy(preview, resp.c_str(), sizeof(preview) - 1);
      preview[sizeof(preview) - 1] = 0;
      PushChunk(preview);
    }
    state_ = ClientState::Done;
  }
  http.end();
  return state_ != ClientState::Error;
}

bool AIClient::DoRest(const char* prompt, bool stream) {
  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  bool began = false;

  if (cfg_.use_https) {
    secure.setInsecure();
    secure.setTimeout(25000);
    began = http.begin(secure, cfg_.rest_host, cfg_.rest_port, cfg_.rest_path, true);
  } else {
    began = http.begin(plain, cfg_.rest_host, cfg_.rest_port, cfg_.rest_path, false);
  }

  if (!began) {
    SetError("http begin");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  if (cfg_.api_key[0]) {
    char auth[96];
    snprintf(auth, sizeof(auth), "Bearer %s", cfg_.api_key);
    http.addHeader("Authorization", auth);
  }
  http.setTimeout(30000);
  http.setConnectTimeout(15000);

  String body;
  body.reserve(640);
  body += "{\"model\":\"";
  body += cfg_.model;
  body += "\",\"stream\":";
  body += stream ? "true" : "false";
  body += ",\"messages\":[{\"role\":\"user\",\"content\":\"";
  for (const char* p = prompt ? prompt : ""; *p; ++p) {
    if (*p == '"' || *p == '\\') body += '\\';
    if (*p == '\n')
      body += "\\n";
    else
      body += *p;
  }
  body += "\"}]}";

  state_ = ClientState::Streaming;
  const int code = http.POST(body);
  if (code < 200 || code >= 300) {
    char err[48];
    if (code == 401 || code == 403) {
      snprintf(err, sizeof(err), "HTTP %d auth", code);
    } else {
      snprintf(err, sizeof(err), "HTTP %d", code);
    }
    http.end();
    SetError(err);
    return false;
  }

  if (stream) {
    WiFiClient* sock = http.getStreamPtr();
    if (!sock) {
      http.end();
      SetError("no stream");
      return false;
    }
    const uint32_t start = millis();
    while (http.connected() && (millis() - start < 45000U)) {
      while (sock->available()) {
        String line = sock->readStringUntil('\n');
        ParseSseOrJsonChunk(line.c_str(), line.length());
        if (state_ == ClientState::Done) break;
      }
      if (state_ == ClientState::Done) break;
      delay(1);
    }
    if (state_ != ClientState::Done && state_ != ClientState::Error) {
      state_ = ClientState::Done;
    }
  } else {
    String resp = http.getString();
    char full[kMaxResponseChars];
    if (ExtractJsonContentString(resp.c_str(), full, sizeof(full))) {
      PushTextAsChunks(full);
    } else {
      char preview[kMaxStreamChunk];
      strncpy(preview, resp.c_str(), sizeof(preview) - 1);
      preview[sizeof(preview) - 1] = 0;
      PushChunk(preview);
    }
    state_ = ClientState::Done;
  }
  http.end();
  return state_ != ClientState::Error;
}

bool AIClient::DoWebSocket(const char* prompt, bool stream) {
  (void)stream;
  if (cfg_.use_https) {
    SetError("WS+TLS N/A");
    return false;
  }
  g_ws_owner = this;
  g_ws.begin(cfg_.ws_host, cfg_.ws_port, cfg_.ws_path);
  g_ws.onEvent(WsEvent);
  g_ws.setReconnectInterval(3000);
  ws_active_ = true;
  state_ = ClientState::Connecting;

  const uint32_t start = millis();
  while (millis() - start < 5000U) {
    g_ws.loop();
    if (g_ws.isConnected()) break;
    delay(10);
  }
  if (!g_ws.isConnected()) {
    SetError("ws connect");
    ws_active_ = false;
    return false;
  }

  String msg;
  msg.reserve(384);
  msg += "{\"type\":\"chat\",\"stream\":true,\"model\":\"";
  msg += cfg_.model;
  msg += "\",\"content\":\"";
  for (const char* p = prompt ? prompt : ""; *p; ++p) {
    if (*p == '"' || *p == '\\') msg += '\\';
    if (*p == '\n')
      msg += "\\n";
    else
      msg += *p;
  }
  msg += "\"}";
  g_ws.sendTXT(msg);
  state_ = ClientState::Streaming;

  const uint32_t t0 = millis();
  uint32_t last_chunk_ms = 0;
  while (millis() - t0 < 20000U) {
    g_ws.loop();
    delay(5);
    if (chunk_count_ > 0) last_chunk_ms = millis();
    if (chunk_count_ > 0 && last_chunk_ms > 0 && millis() - last_chunk_ms > 800U) {
      state_ = ClientState::Done;
      break;
    }
  }
  if (state_ == ClientState::Streaming) state_ = ClientState::Done;
  g_ws.disconnect();
  ws_active_ = false;
  g_ws_owner = nullptr;
  return true;
}

void AIClient::Tick() {
  if (ws_active_) {
    g_ws.loop();
  }
  if (state_ == ClientState::Streaming && (millis() - request_started_ms_ > 60000U)) {
    SetError("timeout");
    Abort();
  }
}

}  // namespace axiom::ai
