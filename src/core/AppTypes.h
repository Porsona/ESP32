#pragma once
#include <stdint.h>

namespace deskos {
enum class AppId : uint8_t { Home, Pet, Window, Focus, Missions, PcMonitor, Weather, Count, None=255 };
constexpr uint8_t APP_COUNT=static_cast<uint8_t>(AppId::Count);
struct AppInfo { AppId id; const char* name; const char* label; };
const AppInfo& appInfo(AppId id);

struct Rect {
  int16_t x,y,w,h;
  constexpr bool contains(int16_t px,int16_t py) const {
    return px>=x && px<x+w && py>=y && py<y+h;
  }
};

// Separate from the raw TouchEvent flags in M0.2; UI receives calibrated pixels only.
struct TouchEvent {
  enum class Type : uint8_t { Press, Move, Release, Tap, Cancel };
  Type type;
  int16_t x,y;
  uint32_t timestampMs,heldMs;
};
}  // namespace deskos
