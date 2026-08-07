#include "services/storage/storage_service.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>

#include "core/config.h"

namespace axiom::services {

bool StorageService::Begin() {
  chip_flash_bytes_ = ESP.getFlashChipSize();
  EnsureInternal();
  // SD shares CS with nRF CSN — mount on demand from UI
  return true;
}

void StorageService::Tick() {}

bool StorageService::EnsureInternal() {
  if (internal_mounted_) return true;
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    // fallback label "spiffs" partition name used by many default tables
    if (!LittleFS.begin(true)) {
      internal_mounted_ = false;
      return false;
    }
  }
  internal_mounted_ = true;
  if (!LittleFS.exists("/readme.txt")) {
    File f = LittleFS.open("/readme.txt", FILE_WRITE);
    if (f) {
      f.println("AxiomOS Cardputer Edition");
      f.println("Внутренняя флеш LittleFS");
      f.close();
    }
  }
  return true;
}

bool StorageService::MountSd() {
  if (sd_mounted_) return true;
  if (!sd_spi_ready_) {
    SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
    sd_spi_ready_ = true;
  }
  if (!SD.begin(kSdCsPin, SPI, 25000000)) {
    sd_mounted_ = false;
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    sd_mounted_ = false;
    return false;
  }
  sd_mounted_ = true;
  return true;
}

void StorageService::UnmountSd() {
  if (!sd_mounted_) return;
  SD.end();
  sd_mounted_ = false;
}

FsStats StorageService::GetStats(FsVolume vol) const {
  FsStats s;
  if (vol == FsVolume::Internal) {
    strncpy(s.label, "Флеш", sizeof(s.label) - 1);
    s.mounted = internal_mounted_;
    if (internal_mounted_) {
      s.total_bytes = LittleFS.totalBytes();
      s.used_bytes = LittleFS.usedBytes();
      s.free_bytes = s.total_bytes > s.used_bytes ? s.total_bytes - s.used_bytes : 0;
    }
  } else {
    strncpy(s.label, "SD", sizeof(s.label) - 1);
    s.mounted = sd_mounted_;
    if (sd_mounted_) {
      s.total_bytes = SD.totalBytes();
      s.used_bytes = SD.usedBytes();
      s.free_bytes = s.total_bytes > s.used_bytes ? s.total_bytes - s.used_bytes : 0;
    }
  }
  return s;
}

void StorageService::SetVolume(FsVolume vol) {
  volume_ = vol;
  strncpy(path_, "/", sizeof(path_) - 1);
  path_[sizeof(path_) - 1] = '\0';
  entry_count_ = 0;
}

bool StorageService::List(FsVolume vol, const char* path) {
  volume_ = vol;
  if (path != nullptr) {
    strncpy(path_, path, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';
  }
  entry_count_ = 0;

  fs::FS* fs = nullptr;
  if (vol == FsVolume::Internal) {
    if (!EnsureInternal()) return false;
    fs = &LittleFS;
  } else {
    if (!MountSd()) return false;
    fs = &SD;
  }

  File root = fs->open(path_);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return false;
  }

  File file = root.openNextFile();
  while (file && entry_count_ < kMaxEntries) {
    FsEntry& e = entries_[entry_count_];
    const char* n = file.name();
    const char* base = strrchr(n, '/');
    base = base ? base + 1 : n;
    strncpy(e.name, base, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
    e.is_dir = file.isDirectory();
    e.size = e.is_dir ? 0 : static_cast<uint32_t>(file.size());
    ++entry_count_;
    file = root.openNextFile();
  }
  root.close();
  return true;
}

bool StorageService::EnterDir(const char* name) {
  if (name == nullptr || name[0] == '\0') return false;
  char next[64];
  if (strcmp(path_, "/") == 0) {
    snprintf(next, sizeof(next), "/%s", name);
  } else {
    snprintf(next, sizeof(next), "%s/%s", path_, name);
  }
  return List(volume_, next);
}

bool StorageService::GoUp() {
  if (strcmp(path_, "/") == 0) return List(volume_, "/");
  char* slash = strrchr(path_, '/');
  if (slash == nullptr) return List(volume_, "/");
  if (slash == path_) {
    path_[1] = '\0';
  } else {
    *slash = '\0';
  }
  return List(volume_, path_);
}

}  // namespace axiom::services
