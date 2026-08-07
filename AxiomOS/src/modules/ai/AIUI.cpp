#include "modules/ai/AIUI.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace axiom::ai {
namespace {
constexpr const char* kAiMenuItems[] = {"Dashboard", "Chat",     "Agents", "Knowledge",
                                        "Memory",    "History",  "Settings"};
constexpr int kAiMenuCount = 7;

void Trunc(char* dst, size_t n, const char* src, size_t max_chars) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = 0;
    return;
  }
  size_t i = 0;
  for (; src[i] && i < max_chars && i + 1 < n; ++i) dst[i] = src[i];
  if (src[i] && i + 1 < n) {
    if (i >= 1) dst[i - 1] = '.';
    if (i >= 2) dst[i - 2] = '.';
    if (i >= 3) dst[i - 3] = '.';
  }
  dst[i] = 0;
}
}  // namespace

void AIUI::Open() {
  active_ = true;
  screen_ = AiScreen::Submenu;
  selected_ = 0;
  scroll_ = 0;
  compose_mode_ = false;
  compose_len_ = 0;
  compose_[0] = 0;
  Refresh(true);
}

void AIUI::Close() {
  active_ = false;
  screen_ = AiScreen::Submenu;
  compose_mode_ = false;
}

const char* AIUI::Breadcrumb() const {
  switch (screen_) {
    case AiScreen::Dashboard:
      return "AI / Dashboard";
    case AiScreen::Chat:
      return "AI / Chat";
    case AiScreen::Agents:
      return "AI / Agents";
    case AiScreen::Knowledge:
      return "AI / Knowledge";
    case AiScreen::Memory:
      return "AI / Memory";
    case AiScreen::History:
      return "AI / History";
    case AiScreen::Settings:
      return "AI / Settings";
    case AiScreen::Confirm:
      return "AI / Confirm";
    default:
      return "AI";
  }
}

void AIUI::HideExtra(int shown) {
  if (!host_.rows || !host_.texts) return;
  for (int i = 0; i < 6; ++i) {
    if (i < shown) {
      lv_obj_remove_flag(host_.rows[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(host_.rows[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void AIUI::LayoutCursor(int focus_row) {
  if (!host_.cursor) return;
  lv_obj_clear_flag(host_.cursor, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(host_.cursor, focus_row * host_.row_step);
  lv_obj_set_style_opa(host_.cursor, LV_OPA_COVER, 0);
}

void AIUI::SetRows(const char* lines[], int count, int focus, bool animate_in) {
  (void)animate_in;
  if (!host_.rows || !host_.texts) return;
  if (focus < scroll_) scroll_ = focus;
  if (focus >= scroll_ + host_.visible_rows) scroll_ = focus - host_.visible_rows + 1;
  if (scroll_ < 0) scroll_ = 0;

  int shown = 0;
  for (int row = 0; row < 6; ++row) {
    const int idx = scroll_ + row;
    if (row >= host_.visible_rows || idx >= count) {
      lv_obj_add_flag(host_.rows[row], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    ++shown;
    lv_obj_remove_flag(host_.rows[row], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(host_.texts[row], lines[idx] ? lines[idx] : "");
    lv_obj_set_style_text_color(host_.texts[row],
                                lv_color_hex(idx == focus ? 0xEAFBFF : 0x9EB0C4), 0);
  }
  LayoutCursor(focus - scroll_);
  if (host_.breadcrumb) lv_label_set_text(host_.breadcrumb, Breadcrumb());
}

void AIUI::OpenScreen(AiScreen s) {
  screen_ = s;
  selected_ = 0;
  scroll_ = 0;
  if (s == AiScreen::Chat) {
    compose_mode_ = true;
    compose_len_ = 0;
    compose_[0] = 0;
    chat_follow_ = true;
    chat_scroll_ = 0;
    chat_last_msg_count_ = 0;
    chat_last_stream_len_ = 0;
  } else {
    compose_mode_ = false;
  }
  Refresh(true);
}

void AIUI::Refresh(bool animate_in) {
  if (!active_ || !mgr_) return;
  if (mgr_->Actions().HasPending() && screen_ != AiScreen::Confirm) {
    screen_ = AiScreen::Confirm;
  }
  switch (screen_) {
    case AiScreen::Dashboard:
      RefreshDashboard(animate_in);
      break;
    case AiScreen::Chat:
      RefreshChat(animate_in);
      break;
    case AiScreen::Agents:
      RefreshAgents(animate_in);
      break;
    case AiScreen::Knowledge:
      RefreshKnowledge(animate_in);
      break;
    case AiScreen::Memory:
      RefreshMemory(animate_in);
      break;
    case AiScreen::History:
      RefreshHistory(animate_in);
      break;
    case AiScreen::Settings:
      RefreshSettings(animate_in);
      break;
    case AiScreen::Confirm:
      RefreshConfirm(animate_in);
      break;
    case AiScreen::Submenu:
    default:
      RefreshSubmenu(animate_in);
      break;
  }
}

void AIUI::RefreshSubmenu(bool animate_in) {
  const char* lines[8];
  for (int i = 0; i < kAiMenuCount; ++i) lines[i] = kAiMenuItems[i];
  SetRows(lines, kAiMenuCount, selected_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, ";/. Enter Del | AI");
}

void AIUI::RefreshDashboard(bool animate_in) {
  static char lines[5][48];
  const char* ptrs[5];

  AgentReport rec;
  const bool has = mgr_->BestRecommendation(rec);
  snprintf(lines[0], sizeof(lines[0]), "AxiomOS AI %s",
           mgr_->Enabled() ? "ON" : "OFF");
  snprintf(lines[1], sizeof(lines[1]), "%s", mgr_->LastStatus());

  char wifi[24] = "WiFi —";
  char nrf[24] = "nRF —";
  // Soft status from last agent sweep
  AgentReport net, rf;
  if (mgr_->RunAgentNow(AgentId::Network, net)) {
    Trunc(wifi, sizeof(wifi), net.ok ? "WiFi OK" : "WiFi !", 20);
  }
  if (mgr_->RunAgentNow(AgentId::Rf, rf)) {
    Trunc(nrf, sizeof(nrf), rf.ok ? "nRF Ready" : "nRF !", 20);
  }
  snprintf(lines[2], sizeof(lines[2]), "%s  %s", wifi, nrf);

  if (has) {
    Trunc(lines[3], sizeof(lines[3]), rec.summary, 36);
    Trunc(lines[4], sizeof(lines[4]), rec.recommendation, 36);
  } else {
    snprintf(lines[3], sizeof(lines[3]), "Нет рекомендаций");
    snprintf(lines[4], sizeof(lines[4]), "Запусти Agents/Doctor");
  }
  for (int i = 0; i < 5; ++i) ptrs[i] = lines[i];
  SetRows(ptrs, 5, selected_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Enter Chat  R Agents  Del");
}

void AIUI::AppendWrapped(char prefix, const char* text, uint16_t max_chars) {
  if (!text) text = "";
  char tmp[kMaxMessageChars + 1];
  const uint16_t src_len = static_cast<uint16_t>(strlen(text));
  const uint16_t use = (max_chars < src_len) ? max_chars : src_len;
  memcpy(tmp, text, use);
  tmp[use] = 0;

  // Collapse newlines to spaces for wrap, but keep paragraph breaks as hard wraps
  for (uint16_t i = 0; i < use; ++i) {
    if (tmp[i] == '\r') tmp[i] = ' ';
  }

  const char* p = tmp;
  bool first = true;
  while (*p && chat_line_count_ < kChatViewMaxLines) {
    while (*p == ' ') ++p;
    if (*p == '\n') {
      ++p;
      if (chat_line_count_ < kChatViewMaxLines) {
        snprintf(chat_lines_[chat_line_count_], sizeof(chat_lines_[0]), "%c",
                 first ? prefix : ' ');
        ++chat_line_count_;
      }
      first = false;
      continue;
    }
    if (!*p) break;

    const int indent = 2;  // "> " or "  "
    const int width = static_cast<int>(kChatWrapWidth) - indent;
    if (width < 8) break;

    // Find break point
    int take = 0;
    int last_space = -1;
    for (int i = 0; p[i] && p[i] != '\n' && i < width; ++i) {
      take = i + 1;
      if (p[i] == ' ') last_space = i;
    }
    if (p[take] && p[take] != '\n' && last_space > 8) {
      take = last_space;
    }
    if (take <= 0) take = 1;

    char* dst = chat_lines_[chat_line_count_];
    if (first) {
      snprintf(dst, sizeof(chat_lines_[0]), "%c %.*s", prefix, take, p);
    } else {
      snprintf(dst, sizeof(chat_lines_[0]), "  %.*s", take, p);
    }
    ++chat_line_count_;
    first = false;
    p += take;
    while (*p == ' ') ++p;
  }
}

void AIUI::RebuildChatLines() {
  chat_line_count_ = 0;
  if (!mgr_) return;
  auto& conv = mgr_->Conversation();

  const uint16_t nmsg = conv.Count();
  for (uint16_t i = 0; i < nmsg && chat_line_count_ < kChatViewMaxLines; ++i) {
    const auto& m = conv.At(i);
    const char prefix = (m.role == ChatRole::User) ? '>' : '<';
    uint16_t lim = static_cast<uint16_t>(strlen(m.text));
    if (i == nmsg - 1 && m.role == ChatRole::Assistant &&
        conv.State() == ConversationState::Typing) {
      lim = conv.TypingVisibleChars();
      const uint16_t tl = static_cast<uint16_t>(strlen(m.text));
      if (lim > tl) lim = tl;
    }
    AppendWrapped(prefix, m.text, lim);
  }

  if (conv.State() == ConversationState::Streaming && conv.StreamingLen() > 0) {
    AppendWrapped('<', conv.StreamingBuffer(), conv.StreamingLen());
  } else if (mgr_->Thinking() || conv.State() == ConversationState::Thinking) {
    if (chat_line_count_ < kChatViewMaxLines) {
      snprintf(chat_lines_[chat_line_count_], sizeof(chat_lines_[0]), "... AI думает");
      ++chat_line_count_;
    }
  }
}

void AIUI::EnsureChatFollow() {
  if (!mgr_) return;
  auto& conv = mgr_->Conversation();
  const uint16_t n = conv.Count();
  const uint16_t sl = conv.StreamingLen();
  if (n != chat_last_msg_count_ || sl != chat_last_stream_len_) {
    chat_last_msg_count_ = n;
    chat_last_stream_len_ = sl;
    chat_follow_ = true;
  }
}

void AIUI::RefreshChat(bool animate_in) {
  RebuildChatLines();
  EnsureChatFollow();

  const int vis = host_.visible_rows > 0 ? host_.visible_rows : 4;
  const int compose_rows = compose_mode_ ? 1 : 0;
  const int body_rows = vis - compose_rows;
  const int max_scroll =
      (chat_line_count_ > static_cast<uint16_t>(body_rows))
          ? static_cast<int>(chat_line_count_) - body_rows
          : 0;

  if (chat_follow_) chat_scroll_ = static_cast<uint16_t>(max_scroll);
  if (chat_scroll_ > max_scroll) chat_scroll_ = static_cast<uint16_t>(max_scroll);

  static char lines[6][48];
  const char* ptrs[6];
  int count = 0;

  for (int row = 0; row < body_rows && count < 6; ++row) {
    const int idx = static_cast<int>(chat_scroll_) + row;
    if (idx < static_cast<int>(chat_line_count_)) {
      strncpy(lines[count], chat_lines_[idx], sizeof(lines[0]) - 1);
      lines[count][sizeof(lines[0]) - 1] = 0;
    } else {
      lines[count][0] = 0;
    }
    ptrs[count] = lines[count];
    ++count;
  }

  if (compose_mode_ && count < 6) {
    snprintf(lines[count], sizeof(lines[count]), "> %s_", compose_);
    ptrs[count] = lines[count];
    selected_ = count;
    ++count;
  }

  if (count == 0) {
    snprintf(lines[0], sizeof(lines[0]), "Печатай сообщение");
    ptrs[0] = lines[0];
    count = 1;
    selected_ = 0;
  }

  SetRows(ptrs, count, selected_, animate_in);

  if (host_.hint) {
    if (mgr_->Thinking() || mgr_->Conversation().State() == ConversationState::Streaming) {
      lv_label_set_text(host_.hint, "Ждите...");
    } else {
      char hint[40];
      snprintf(hint, sizeof(hint), "Fn+;/. скролл %u/%u",
               static_cast<unsigned>(chat_scroll_ + 1),
               static_cast<unsigned>(chat_line_count_ == 0 ? 1 : chat_line_count_));
      lv_label_set_text(host_.hint, hint);
    }
  }
}

void AIUI::RefreshAgents(bool animate_in) {
  static char lines[8][48];
  const char* ptrs[8];
  const int n = mgr_->AgentCount();
  for (int i = 0; i < n; ++i) {
    IAgent* a = mgr_->AgentAt(static_cast<uint8_t>(i));
    snprintf(lines[i], sizeof(lines[i]), "%s", a ? a->Name() : "?");
    ptrs[i] = lines[i];
  }
  snprintf(lines[n], sizeof(lines[n]), "Device Doctor");
  ptrs[n] = lines[n];
  SetRows(ptrs, n + 1, selected_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Enter анализ  Del");
}

void AIUI::RefreshKnowledge(bool animate_in) {
  static char lines[16][48];
  const char* ptrs[16];
  const uint16_t n = mgr_->Knowledge().ArticleCount();
  for (uint16_t i = 0; i < n; ++i) {
    const auto* a = mgr_->Knowledge().ArticleAt(i);
    snprintf(lines[i], sizeof(lines[i]), "%s", a ? a->title : "?");
    ptrs[i] = lines[i];
  }
  SetRows(ptrs, static_cast<int>(n), selected_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Enter читать  Del");
}

void AIUI::RefreshMemory(bool animate_in) {
  static char lines[8][48];
  const char* ptrs[8];
  auto& mem = mgr_->Memory();
  int count = 0;
  snprintf(lines[count], sizeof(lines[count]), "Факты: %u", mem.FactCount());
  ptrs[count++] = lines[0];
  const uint8_t fc = mem.FactCount();
  for (uint8_t i = 0; i < fc && count < 5; ++i) {
    const auto& f = mem.FactAt(i);
    snprintf(lines[count], sizeof(lines[count]), "%s=%s", f.key, f.value);
    ptrs[count] = lines[count];
    ++count;
  }
  if (count == 1) {
    snprintf(lines[1], sizeof(lines[1]), "(пусто)");
    ptrs[1] = lines[1];
    count = 2;
  }
  SetRows(ptrs, count, selected_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Память NVS/FS  Del");
}

void AIUI::RefreshHistory(bool animate_in) {
  static char buf[512];
  static char lines[5][48];
  const char* ptrs[5];
  mgr_->Memory().LoadHistoryTail(buf, sizeof(buf), 5);
  if (buf[0] == 0) {
    snprintf(lines[0], sizeof(lines[0]), "История пуста");
    ptrs[0] = lines[0];
    SetRows(ptrs, 1, 0, animate_in);
  } else {
    int count = 0;
    char* save = nullptr;
    for (char* tok = strtok_r(buf, "\n", &save); tok && count < 5;
         tok = strtok_r(nullptr, "\n", &save)) {
      Trunc(lines[count], sizeof(lines[count]), tok, 36);
      ptrs[count] = lines[count];
      ++count;
    }
    SetRows(ptrs, count, selected_, animate_in);
  }
  if (host_.hint) lv_label_set_text(host_.hint, "R очистить  Del");
}

void AIUI::RefreshSettings(bool animate_in) {
  static char lines[8][48];
  const char* ptrs[8];
  const auto& s = mgr_->Settings().Data();
  snprintf(lines[0], sizeof(lines[0]), "AI          %s", s.enabled ? "ON" : "OFF");
  snprintf(lines[1], sizeof(lines[1]), "Local only  %s", s.local_only ? "YES" : "NO");
  snprintf(lines[2], sizeof(lines[2]), "Transport   %s",
           s.transport == AiTransport::Rest ? "REST" : "WS");
  snprintf(lines[3], sizeof(lines[3]), "Stream      %s", s.stream ? "ON" : "OFF");
  snprintf(lines[4], sizeof(lines[4]), "Confirm     %s", s.confirm_actions ? "ON" : "OFF");
  snprintf(lines[5], sizeof(lines[5]), "Host %s", s.rest_host);
  snprintf(lines[6], sizeof(lines[6]), "Model %s", s.model);
  snprintf(lines[7], sizeof(lines[7]), "Voice ready %s", s.voice_ready ? "YES" : "NO");
  for (int i = 0; i < 8; ++i) ptrs[i] = lines[i];
  SetRows(ptrs, 8, settings_index_, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Enter toggle  Del");
}

void AIUI::RefreshConfirm(bool animate_in) {
  static char lines[3][48];
  const char* ptrs[3];
  const auto& p = mgr_->Actions().Pending();
  snprintf(lines[0], sizeof(lines[0]), "Действие: %s", AIActions::Name(p.id));
  snprintf(lines[1], sizeof(lines[1]), "Enter = ДА");
  snprintf(lines[2], sizeof(lines[2]), "Del = НЕТ");
  ptrs[0] = lines[0];
  ptrs[1] = lines[1];
  ptrs[2] = lines[2];
  SetRows(ptrs, 3, 1, animate_in);
  if (host_.hint) lv_label_set_text(host_.hint, "Подтверждение");
}

void AIUI::CommitChat() {
  if (compose_len_ == 0 || !mgr_) return;
  mgr_->SubmitChat(compose_);
  compose_len_ = 0;
  compose_[0] = 0;
  chat_follow_ = true;
  Refresh(false);
}

bool AIUI::HandleChatInput(drivers::InputAction action, char ch) {
  const int vis = host_.visible_rows > 0 ? host_.visible_rows : 4;
  const int body_rows = vis - (compose_mode_ ? 1 : 0);

  switch (action) {
    case drivers::InputAction::Up: {
      RebuildChatLines();
      chat_follow_ = false;
      if (chat_scroll_ > 0) --chat_scroll_;
      Refresh(false);
      return true;
    }
    case drivers::InputAction::Down: {
      RebuildChatLines();
      const int max_scroll =
          (chat_line_count_ > static_cast<uint16_t>(body_rows))
              ? static_cast<int>(chat_line_count_) - body_rows
              : 0;
      if (static_cast<int>(chat_scroll_) < max_scroll) {
        ++chat_scroll_;
        chat_follow_ = (static_cast<int>(chat_scroll_) >= max_scroll);
      } else {
        chat_follow_ = true;
      }
      Refresh(false);
      return true;
    }
    case drivers::InputAction::Char:
      if (compose_len_ + 1 < sizeof(compose_)) {
        compose_[compose_len_++] = ch;
        compose_[compose_len_] = 0;
        chat_follow_ = true;
        Refresh(false);
      }
      return true;
    case drivers::InputAction::DeleteChar:
      if (compose_len_ > 0) {
        compose_[--compose_len_] = 0;
        Refresh(false);
      }
      return true;
    case drivers::InputAction::Select:
      CommitChat();
      return true;
    case drivers::InputAction::Rescan:
      chat_follow_ = true;
      Refresh(false);
      return true;
    case drivers::InputAction::Back:
      if (compose_len_ > 0) {
        compose_len_ = 0;
        compose_[0] = 0;
        Refresh(false);
        return true;
      }
      OpenScreen(AiScreen::Submenu);
      return true;
    default:
      return false;
  }
}

bool AIUI::HandleSettingsInput(drivers::InputAction action) {
  auto& st = mgr_->Settings();
  const int max_i = 7;
  switch (action) {
    case drivers::InputAction::Up:
      settings_index_ = (settings_index_ - 1 + max_i + 1) % (max_i + 1);
      Refresh(false);
      return true;
    case drivers::InputAction::Down:
      settings_index_ = (settings_index_ + 1) % (max_i + 1);
      Refresh(false);
      return true;
    case drivers::InputAction::Select: {
      auto& d = st.Data();
      switch (settings_index_) {
        case 0:
          st.SetEnabled(!d.enabled);
          break;
        case 1:
          st.SetLocalOnly(!d.local_only);
          break;
        case 2:
          st.CycleTransport();
          break;
        case 3:
          st.SetStream(!d.stream);
          break;
        case 4:
          st.SetConfirmActions(!d.confirm_actions);
          mgr_->Actions().SetRequireConfirm(st.Data().confirm_actions);
          break;
        case 7:
          d.voice_ready = !d.voice_ready;
          st.Save();
          mgr_->Voice().SetEnabled(d.voice_ready);
          break;
        default:
          break;
      }
      Refresh(false);
      return true;
    }
    case drivers::InputAction::Back:
      OpenScreen(AiScreen::Submenu);
      return true;
    default:
      return true;
  }
}

bool AIUI::HandleInput(drivers::InputAction action, char ch) {
  if (!active_ || !mgr_) return false;

  if (screen_ == AiScreen::Confirm) {
    if (action == drivers::InputAction::Select) {
      mgr_->Actions().Confirm();
      mgr_->Conversation().Add(ChatRole::Assistant, "Действие выполнено.");
      screen_ = AiScreen::Chat;
      Refresh(true);
      return true;
    }
    if (action == drivers::InputAction::Back) {
      mgr_->Actions().Cancel();
      mgr_->Conversation().Add(ChatRole::Assistant, "Отменено.");
      screen_ = AiScreen::Chat;
      Refresh(true);
      return true;
    }
    return true;
  }

  if (screen_ == AiScreen::Chat) return HandleChatInput(action, ch);
  if (screen_ == AiScreen::Settings) return HandleSettingsInput(action);

  const int max_items = [&]() {
    switch (screen_) {
      case AiScreen::Submenu:
        return kAiMenuCount;
      case AiScreen::Dashboard:
        return 5;
      case AiScreen::Agents:
        return mgr_->AgentCount() + 1;
      case AiScreen::Knowledge:
        return static_cast<int>(mgr_->Knowledge().ArticleCount());
      case AiScreen::Memory:
        return 5;
      case AiScreen::History:
        return 5;
      default:
        return 1;
    }
  }();

  switch (action) {
    case drivers::InputAction::Up:
      if (max_items > 0) selected_ = (selected_ - 1 + max_items) % max_items;
      Refresh(false);
      return true;
    case drivers::InputAction::Down:
      if (max_items > 0) selected_ = (selected_ + 1) % max_items;
      Refresh(false);
      return true;
    case drivers::InputAction::Back:
      if (screen_ == AiScreen::Submenu) {
        Close();
        return true;  // UiManager should exit AI section
      }
      OpenScreen(AiScreen::Submenu);
      return true;
    case drivers::InputAction::Rescan:
      if (screen_ == AiScreen::History) {
        mgr_->Memory().ClearHistory();
        Refresh(true);
        return true;
      }
      if (screen_ == AiScreen::Dashboard) {
        OpenScreen(AiScreen::Agents);
        return true;
      }
      return true;
    case drivers::InputAction::Select:
      if (screen_ == AiScreen::Submenu) {
        switch (selected_) {
          case 0:
            OpenScreen(AiScreen::Dashboard);
            break;
          case 1:
            OpenScreen(AiScreen::Chat);
            break;
          case 2:
            OpenScreen(AiScreen::Agents);
            break;
          case 3:
            OpenScreen(AiScreen::Knowledge);
            break;
          case 4:
            OpenScreen(AiScreen::Memory);
            break;
          case 5:
            OpenScreen(AiScreen::History);
            break;
          case 6:
            OpenScreen(AiScreen::Settings);
            break;
          default:
            break;
        }
        return true;
      }
      if (screen_ == AiScreen::Dashboard) {
        OpenScreen(AiScreen::Chat);
        return true;
      }
      if (screen_ == AiScreen::Agents) {
        if (selected_ >= mgr_->AgentCount()) {
          mgr_->SubmitDoctor();
        } else {
          mgr_->SubmitAgent(static_cast<AgentId>(selected_));
        }
        OpenScreen(AiScreen::Chat);
        return true;
      }
      if (screen_ == AiScreen::Knowledge) {
        char body[kMaxKnowledgeHit];
        if (mgr_->Knowledge().ArticleBody(static_cast<uint16_t>(selected_), body, sizeof(body))) {
          mgr_->Conversation().Add(ChatRole::Assistant, body);
          OpenScreen(AiScreen::Chat);
        }
        return true;
      }
      return true;
    default:
      return true;
  }
}

void AIUI::Tick() {
  if (!active_) return;
  if (screen_ == AiScreen::Chat || screen_ == AiScreen::Dashboard) {
    Refresh(false);
  }
}

}  // namespace axiom::ai
