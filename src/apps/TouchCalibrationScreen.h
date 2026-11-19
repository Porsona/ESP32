#pragma once
#include "DisplayHAL.h"
#include "../core/TouchCalibration.h"

class TouchCalibrationScreen {
 public:
  explicit TouchCalibrationScreen(DisplayHAL& display) : display_(display) {}
  void begin(uint32_t now);
  void update(const TouchFrame& frame, uint32_t now);
  bool takeResult(touchcal::CalibrationRecord& result);
  void storageFailed();
 private:
  enum class Stage { Collect, Verify, Failed, Complete };
  void drawTarget();
  void showFailure(const char* message);
  void accept(const RawTouchPoint& median, uint32_t now);
  DisplayHAL& display_;
  Stage stage_ = Stage::Collect;
  uint8_t point_ = 0;
  uint32_t pointAt_ = 0, lastProgress_ = 0;
  bool waitForRelease_ = true, sawPress_ = false, resultTaken_ = false;
  touchcal::StableSamples samples_;
  RawTouchPoint measured_[touchcal::POINT_COUNT];
  RawTouchPoint verification_[touchcal::VERIFY_COUNT];
  touchcal::CalibrationRecord candidate_{};
};
