#include "TimeServiceESP.h"
#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

namespace deskos {
namespace {
volatile bool g_synced=false;
volatile uint32_t g_syncs=0;
void onSync(struct timeval*) { g_synced=true; g_syncs=g_syncs+1; }  // Runs in the lwIP/SNTP task.
}

void TimeServiceESP::start() {
  if (started_) return;
  started_=true;
  sntp_set_time_sync_notification_cb(onSync);
  configTzTime(LOCAL_POSIX_TZ,"pool.ntp.org","time.google.com","time.cloudflare.com");
  Serial.println("TIME: SNTP_STARTED tz=Asia/Ho_Chi_Minh(UTC+7)");
}
uint32_t TimeServiceESP::epoch() const { return static_cast<uint32_t>(time(nullptr)); }
bool TimeServiceESP::valid() const { return g_synced && plausibleEpoch(epoch()); }
bool TimeServiceESP::calendarDay(uint32_t& dayKey) const {
  if (!valid()) return false;
  dayKey=localDayKey(epoch());
  return true;
}
uint32_t TimeServiceESP::syncCount() const { return g_syncs; }
}  // namespace deskos
