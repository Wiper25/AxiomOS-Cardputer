#pragma once

#include <stdint.h>

namespace axiom::services {

enum class FsVolume : uint8_t { Internal = 0, Sd = 1 };

struct FsEntry {
  char name[28] = {0};
  bool is_dir = false;
  uint32_t size = 0;
};

struct FsStats {
  bool mounted = false;
  uint64_t total_bytes = 0;
  uint64_t used_bytes = 0;
  uint64_t free_bytes = 0;
  char label[16] = {0};
};

class StorageService {
 public:
  static constexpr uint8_t kMaxEntries = 24;

  bool Begin();
  void Tick();
  bool MountSd();
  void UnmountSd();
  bool SdMounted() const { return sd_mounted_; }
  bool InternalMounted() const { return internal_mounted_; }

  FsStats GetStats(FsVolume vol) const;
  uint32_t ChipFlashBytes() const { return chip_flash_bytes_; }

  bool List(FsVolume vol, const char* path);
  uint8_t EntryCount() const { return entry_count_; }
  const FsEntry& EntryAt(uint8_t i) const { return entries_[i]; }
  const char* CurrentPath() const { return path_; }
  FsVolume CurrentVolume() const { return volume_; }
  void SetVolume(FsVolume vol);

  bool EnterDir(const char* name);
  bool GoUp();

 private:
  bool EnsureInternal();

  bool internal_mounted_ = false;
  bool sd_mounted_ = false;
  bool sd_spi_ready_ = false;
  uint32_t chip_flash_bytes_ = 0;
  FsVolume volume_ = FsVolume::Internal;
  char path_[64] = "/";
  FsEntry entries_[kMaxEntries];
  uint8_t entry_count_ = 0;
};

}  // namespace axiom::services
