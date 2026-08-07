#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "drivers/keyboard/keyboard_driver.h"
#include "modules/ai/AIManager.h"

namespace axiom::ai {

enum class AiScreen : uint8_t {
  Submenu = 0,
  Dashboard,
  Chat,
  Agents,
  Knowledge,
  Memory,
  History,
  Settings,
  Confirm
};

struct AiUiHost {
  lv_obj_t** rows = nullptr;       // 6
  lv_obj_t** texts = nullptr;      // 6
  lv_obj_t* hint = nullptr;
  lv_obj_t* breadcrumb = nullptr;
  lv_obj_t* cursor = nullptr;
  int visible_rows = 4;
  int32_t row_step = 21;
};

class AIUI {
 public:
  void Bind(AIManager* mgr) { mgr_ = mgr; }
  void SetHost(const AiUiHost& host) { host_ = host; }

  bool Active() const { return active_; }
  AiScreen Screen() const { return screen_; }

  void Open();   // enter AI submenu
  void Close();  // leave AI entirely
  bool HandleInput(drivers::InputAction action, char ch);
  void Tick();
  void Refresh(bool animate_in);
  const char* Breadcrumb() const;

  bool CapturingText() const {
    return active_ && screen_ == AiScreen::Chat && compose_mode_;
  }

 private:
  void OpenScreen(AiScreen s);
  void RefreshSubmenu(bool animate_in);
  void RefreshDashboard(bool animate_in);
  void RefreshChat(bool animate_in);
  void RefreshAgents(bool animate_in);
  void RefreshKnowledge(bool animate_in);
  void RefreshMemory(bool animate_in);
  void RefreshHistory(bool animate_in);
  void RefreshSettings(bool animate_in);
  void RefreshConfirm(bool animate_in);
  void SetRows(const char* lines[], int count, int focus, bool animate_in);
  void HideExtra(int shown);
  void LayoutCursor(int focus_row);

  bool HandleChatInput(drivers::InputAction action, char ch);
  bool HandleSettingsInput(drivers::InputAction action);
  void CommitChat();
  void RebuildChatLines();
  void AppendWrapped(char prefix, const char* text, uint16_t max_chars);
  void EnsureChatFollow();

  AIManager* mgr_ = nullptr;
  AiUiHost host_;
  bool active_ = false;
  AiScreen screen_ = AiScreen::Submenu;
  int selected_ = 0;
  int scroll_ = 0;
  int settings_index_ = 0;
  int agent_index_ = 0;
  int knowledge_index_ = 0;
  int memory_index_ = 0;

  char compose_[96] = {0};
  uint8_t compose_len_ = 0;
  bool compose_mode_ = false;

  char chat_lines_[kChatViewMaxLines][40];
  uint16_t chat_line_count_ = 0;
  uint16_t chat_scroll_ = 0;
  bool chat_follow_ = true;
  uint16_t chat_last_msg_count_ = 0;
  uint16_t chat_last_stream_len_ = 0;
};

}  // namespace axiom::ai
