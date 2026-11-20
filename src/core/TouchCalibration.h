#pragma once
#include <stddef.h>
#include "TouchTypes.h"

namespace touchcal {
constexpr size_t POINT_COUNT = 5;
constexpr size_t VERIFY_COUNT = 3;
extern const TouchPixel TARGETS[POINT_COUNT];
extern const TouchPixel VERIFY_TARGETS[VERIFY_COUNT];

struct CalibrationRecord {
  uint32_t magic;
  uint16_t version, bytes, width, height;
  uint8_t rotation, verificationCount, reserved[2];
  float coefficient[6];  // pixelX=a*rawX+b*rawY+c; pixelY=d*rawX+e*rawY+f
  float rmsError, maxError;
  int16_t raw[POINT_COUNT][2];
  int16_t verificationRaw[VERIFY_COUNT][2];
  float verificationMaxError;
  uint32_t checksum;
};
static_assert(sizeof(CalibrationRecord) == 88, "Calibration record layout changed");

uint32_t checksum(const CalibrationRecord& record);
bool fit(const RawTouchPoint points[POINT_COUNT], CalibrationRecord& result);
bool verify(CalibrationRecord& record, const RawTouchPoint points[VERIFY_COUNT]);
bool validRecord(const CalibrationRecord& record, bool requireVerification = true);
bool project(const CalibrationRecord& record, const RawTouchPoint& raw, float& x, float& y);
TouchPixel map(const CalibrationRecord& record, const RawTouchPoint& raw);
float errorAt(const CalibrationRecord& record, const RawTouchPoint& raw, const TouchPixel& target);

class StableSamples {
 public:
  void reset() { count_ = next_ = 0; }
  void add(const RawTouchPoint& point);
  bool median(RawTouchPoint& result, uint8_t& inliers) const;
  uint8_t count() const { return count_; }
 private:
  RawTouchPoint samples_[31];
  uint8_t count_ = 0, next_ = 0;
};
}  // namespace touchcal
