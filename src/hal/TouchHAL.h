#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "../core/TouchTypes.h"

class TouchHAL {
 public:
  TouchHAL();
  TouchHAL(const TouchHAL&) = delete;
  TouchHAL& operator=(const TouchHAL&) = delete;
  bool begin();
  TouchFrame poll(uint32_t now);
 private:
  SPIClass spi_;
  XPT2046_Touchscreen controller_;
  bool initialized_ = false;
  bool candidate_ = false;
  bool down_ = false;
  bool tapSeen_ = false;
  uint32_t lastPoll_ = 0, candidateAt_ = 0, pressedAt_ = 0;
  uint32_t lastTapAt_ = 0;
  RawTouchPoint last_, origin_;
  int maxMotion_ = 0;
};
