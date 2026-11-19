#pragma once
#include <stdint.h>

struct RawTouchPoint {
  int16_t x, y, z;
  constexpr RawTouchPoint(int16_t px=0, int16_t py=0, int16_t pz=0) : x(px), y(py), z(pz) {}
};
struct TouchPixel {
  int16_t x, y;
  constexpr TouchPixel(int16_t px=0, int16_t py=0) : x(px), y(py) {}
};
enum TouchEvent : uint8_t { TOUCH_NONE = 0, TOUCH_PRESS = 1, TOUCH_RELEASE = 2, TOUCH_TAP = 4 };
struct TouchFrame {
  bool updated = false;
  bool down = false;
  bool rawValid = false;
  RawTouchPoint raw;
  uint8_t events = TOUCH_NONE;
  uint32_t heldMs = 0;
};

// ADC electrical bounds, not calibration min/max. XPT2046 pressure can exceed 4095.
inline bool validRawTouch(const RawTouchPoint& p) {
  return p.x > 0 && p.x < 4095 && p.y > 0 && p.y < 4095 && p.z >= 400 && p.z <= 8190;
}
