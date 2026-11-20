#pragma once
#include "TimeModel.h"
#include "WeatherService.h"
#include "WindowSettings.h"

namespace deskos {
// Pixel Window AUTO: real local hour only from a synced clock; rain only from
// fresh (non-stale) weather data.
class LiveWindowContext : public IWindowContext {
 public:
  LiveWindowContext(const ITimeSource& time,const WeatherService& weather) : time_(time),weather_(weather) {}
  bool clockValid() const override { return time_.valid(); }
  uint8_t localHour() const override { return time_.valid()?deskos::localHour(time_.epoch()):0; }
  bool raining() const override { return weather_.raining(); }
 private:
  const ITimeSource& time_;
  const WeatherService& weather_;
};
}  // namespace deskos
