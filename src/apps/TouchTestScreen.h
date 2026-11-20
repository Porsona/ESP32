#pragma once
#include "DisplayHAL.h"
#include "../core/TouchCalibration.h"

class TouchTestScreen {
 public:
  explicit TouchTestScreen(DisplayHAL& display) : display_(display) {}
  void begin();
  void update(const TouchFrame& frame, const touchcal::CalibrationRecord& calibration, uint32_t now);
 private:
  void drawStatic();
  int hit(const TouchPixel& pixel) const;
  DisplayHAL& display_;
  TouchPixel cursor_;
  bool cursorVisible_=false, waitForRelease_=true;
  uint32_t lastDraw_=0, tapCount_=0;
  int lastHit_=-1;
};
