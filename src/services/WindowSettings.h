#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BlobStore.h"

namespace deskos {
constexpr uint16_t WINDOW_SCHEMA_VERSION=1;
struct WindowSettingsRecord {
  uint32_t magic;
  uint16_t version,bytes;
  uint8_t scene,speed,reserved[2];   // WindowScene / WindowSpeed values.
  uint32_t checksum;
};
static_assert(sizeof(WindowSettingsRecord)==16,"Change window schema version when the layout changes");
WindowSettingsRecord defaultWindowSettings();
void sealWindowSettings(WindowSettingsRecord& record);
bool validWindowSettings(const WindowSettingsRecord& record);
BlobLoad loadWindowSettings(IBlobStore& store,WindowSettingsRecord& out);  // Defaults unless Loaded.
bool saveWindowSettings(IBlobStore& store,const WindowSettingsRecord& record);

// What the window may know about the real world. Without a synced clock AUTO
// never guesses day/night (M5 provides TimeService/Weather implementations).
class IWindowContext {
 public:
  virtual ~IWindowContext() = default;
  virtual bool clockValid() const = 0;
  virtual uint8_t localHour() const = 0;
  virtual bool raining() const = 0;
};
class NoWindowContext : public IWindowContext {
 public:
  bool clockValid() const override { return false; }
  uint8_t localHour() const override { return 0; }
  bool raining() const override { return false; }
};
}  // namespace deskos
