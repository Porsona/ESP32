#pragma once
#include "../services/MissionStorage.h"
#include "../services/TimeModel.h"

namespace deskos {
// SNTP (pool.ntp.org/time.google.com/time.cloudflare.com) + TZ Asia/Ho_Chi_Minh.
// valid() is true only after an SNTP sync in this boot, so a stale RTC value
// after a soft reset is never treated as real time.
class TimeServiceESP : public ITimeSource, public IDaySource {
 public:
  void start();                    // Call once the station has an IP. Non-blocking.
  bool started() const { return started_; }
  bool valid() const override;
  uint32_t epoch() const override;
  bool calendarDay(uint32_t& dayKey) const override;
  uint32_t syncCount() const;
 private:
  bool started_=false;
};
}  // namespace deskos
