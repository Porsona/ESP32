#include "DisplayHAL.h"

#include <Arduino.h>
#include "board_pins.h"

// TFT pins stay in platformio.ini; touch/backlight constants stay in board_pins.h.
static_assert(PIN_BACKLIGHT == TFT_BL, "Backlight pin differs from TFT_BL");

void DisplayHAL::begin() {
  if (initialized_) return;
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  tft_.init();
  tft_.setRotation(1);
  tft_.setTextWrap(false);
  initialized_ = true;
}

void DisplayHAL::applyColorConfig() {
  if (!initialized_ || colorConfigApplied_) return;
  tft_.invertDisplay(true);
  colorConfigApplied_ = true;
}
