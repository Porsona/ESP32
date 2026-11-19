#pragma once
#include "FocusStorage.h"
#include "NetTypes.h"
#include "TimeModel.h"
#include "WeatherModel.h"

namespace deskos {
enum class WeatherError : uint8_t { None, Timeout, Transport, Http, Parse, Busy };
const char* weatherErrorName(WeatherError error);
enum class WeatherSource : uint8_t { None, Cache, Live };

// Non-blocking: the fetcher runs the HTTPS request on its own task; this class
// only schedules, validates and caches. Offline it keeps showing the cache.
class WeatherService {
 public:
  static constexpr uint32_t REFRESH_MS=15UL*60UL*1000UL;   // Open-Meteo "current" is 15-minute data.
  static constexpr uint32_t RETRY_BASE_MS=30000,RETRY_MAX_MS=10UL*60UL*1000UL;
  static constexpr uint32_t WATCHDOG_MS=30000;             // Fetcher hard timeouts are shorter.
  static constexpr uint32_t CLOCK_GRACE_MS=20000;          // Prefer an NTP time stamp for the cache.
  static constexpr uint32_t MANUAL_MIN_GAP_MS=30000;
  static constexpr uint32_t STALE_AFTER_S=3UL*3600UL;
  WeatherService(IFocusClock& clock,IBlobStore& store,IHttpFetcher& fetcher,
                 const INetworkStatus& network,const ITimeSource& time)
      : clock_(clock),store_(store),fetcher_(fetcher),network_(network),time_(time) {}
  void begin();
  void update();
  bool requestRefresh();                      // False when busy or rate limited.
  bool setLocation(const WeatherLocation& location);
  const WeatherLocation& location() const { return location_; }
  const WeatherReading& reading() const { return reading_; }
  bool hasReading() const { return reading_.valid==1; }
  WeatherSource source() const { return source_; }
  bool fetching() const { return busy_; }
  WeatherError lastError() const { return error_; }
  int lastHttpCode() const { return httpCode_; }
  uint8_t failures() const { return failures_; }
  uint32_t msUntilNextAttempt() const;
  bool ageSeconds(uint32_t& age) const;       // False when unknown (cache without synced clock).
  bool stale() const;
  bool raining() const { return hasReading() && !stale() && wetWeather(weatherKind(reading_.code)); }
  uint32_t revision() const { return revision_; }
  uint32_t successes() const { return successes_; }
 private:
  void start(uint32_t now);
  void finish(const FetchResult& result,uint32_t now);
  void fail(WeatherError error,uint32_t now);
  IFocusClock& clock_;
  IBlobStore& store_;
  IHttpFetcher& fetcher_;
  const INetworkStatus& network_;
  const ITimeSource& time_;
  WeatherLocation location_{};
  WeatherReading reading_{};
  WeatherSource source_=WeatherSource::None;
  WeatherError error_=WeatherError::None;
  int httpCode_=0;
  bool busy_=false,wasConnected_=false,manual_=false,attempted_=false,discardResult_=false;
  uint8_t failures_=0;
  uint32_t requestId_=0,attemptAt_=0,nextAttemptAt_=0,fetchedAtMs_=0,fetchedEpoch_=0,
           connectedAt_=0,revision_=0,successes_=0;
  char url_[256]={0};
};
}  // namespace deskos
