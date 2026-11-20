#pragma once

#include <TFT_eSPI.h>

class DisplayHAL {
 public:
  // Keep the tested two-stage order: TFT begin -> touch begin -> color config.
  void begin();
  void applyColorConfig();
  TFT_eSPI& graphics() { return tft_; }

 private:
  TFT_eSPI tft_;
  bool initialized_ = false;
  bool colorConfigApplied_ = false;
};
