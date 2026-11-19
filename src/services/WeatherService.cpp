#include "WeatherService.h"
#include <stdio.h>
#include <string.h>

namespace deskos {
namespace { const char* const NS="deskos-wthr"; }

const char* weatherErrorName(WeatherError e) {
  switch (e) {
    case WeatherError::None: return "NONE";
    case WeatherError::Timeout: return "TIMEOUT";
    case WeatherError::Transport: return "NETWORK/TLS";
    case WeatherError::Http: return "HTTP";
    case WeatherError::Parse: return "BAD DATA";
    default: return "BUSY";
  }
}
const char* netStateName(NetState s) {
  switch (s) {
    case NetState::NoConfig: return "NO CONFIG";
    case NetState::Connecting: return "CONNECTING";
    case NetState::Connected: return "CONNECTED";
    default: return "OFF";
  }
}
void formatIPv4(uint32_t a,char* out,size_t size) {
  snprintf(out,size,"%u.%u.%u.%u",static_cast<unsigned>(a&0xFF),static_cast<unsigned>((a>>8)&0xFF),
           static_cast<unsigned>((a>>16)&0xFF),static_cast<unsigned>((a>>24)&0xFF));
}

void WeatherService::begin() {
  location_=defaultLocation();
  WeatherLocationRecord loc;
  if (store_.load(NS,"loc",&loc,sizeof(loc))==BlobLoad::Loaded && validLocationRecord(loc)) location_=loc.location;
  memset(&reading_,0,sizeof(reading_));
  WeatherCacheRecord cache;
  if (store_.load(NS,"cache",&cache,sizeof(cache))==BlobLoad::Loaded && validWeatherCache(cache) &&
      cache.latE4==location_.latE4 && cache.lonE4==location_.lonE4) {
    reading_=cache.reading; fetchedEpoch_=cache.fetchedEpoch; source_=WeatherSource::Cache;  // Offline fallback.
  }
  buildWeatherUrl(location_,url_,sizeof(url_));
  nextAttemptAt_=clock_.nowMs(); ++revision_;
}

uint32_t WeatherService::msUntilNextAttempt() const {
  if (busy_) return 0;
  const int32_t left=static_cast<int32_t>(nextAttemptAt_-clock_.nowMs());
  return left>0?static_cast<uint32_t>(left):0;
}
bool WeatherService::ageSeconds(uint32_t& age) const {
  if (!hasReading()) return false;
  if (source_==WeatherSource::Live) { age=(clock_.nowMs()-fetchedAtMs_)/1000; return true; }
  if (fetchedEpoch_ && time_.valid() && time_.epoch()>=fetchedEpoch_) { age=time_.epoch()-fetchedEpoch_; return true; }
  return false;
}
bool WeatherService::stale() const {
  uint32_t age=0;
  if (!ageSeconds(age)) return true;  // Cache of unknown age is never presented as current.
  return age>STALE_AFTER_S;
}

void WeatherService::start(uint32_t now) {
  ++requestId_;  // Late results from an abandoned request no longer match.
  if (!fetcher_.start(requestId_,url_)) {
    error_=WeatherError::Busy; nextAttemptAt_=now+5000; ++revision_; return;
  }
  busy_=true; attempted_=true; attemptAt_=now; ++revision_;
}
void WeatherService::fail(WeatherError e,uint32_t now) {
  busy_=false; error_=e; discardResult_=false;  // The abandoned request id is never polled again.
  if (failures_<255) ++failures_;
  const uint8_t shift=failures_>5?5:static_cast<uint8_t>(failures_-1);
  uint32_t backoff=RETRY_BASE_MS<<shift;
  if (backoff>RETRY_MAX_MS) backoff=RETRY_MAX_MS;
  nextAttemptAt_=now+backoff; ++revision_;
}
void WeatherService::finish(const FetchResult& r,uint32_t now) {
  busy_=false; httpCode_=r.httpCode;
  if (discardResult_) {  // Requested for the previous location: never show it for the new one.
    discardResult_=false; nextAttemptAt_=now; ++revision_; return;
  }
  if (r.httpCode<=0) { fail(WeatherError::Transport,now); return; }
  if (r.httpCode!=200) { fail(WeatherError::Http,now); return; }
  WeatherReading parsed;
  if (!parseOpenMeteo(r.body,r.length,parsed)) { fail(WeatherError::Parse,now); return; }
  reading_=parsed; source_=WeatherSource::Live; fetchedAtMs_=now;
  fetchedEpoch_=time_.valid()?time_.epoch():0;
  failures_=0; error_=WeatherError::None; nextAttemptAt_=now+REFRESH_MS; ++successes_; ++revision_;
  // One small write per successful refresh (>=15 min apart) keeps flash wear low.
  const WeatherCacheRecord cache=makeWeatherCache(reading_,location_,fetchedEpoch_);
  store_.save(NS,"cache",&cache,sizeof(cache));
}

void WeatherService::update() {
  const uint32_t now=clock_.nowMs();
  const bool connected=network_.connected();
  if (connected!=wasConnected_) {
    wasConnected_=connected; ++revision_;
    if (connected) { connectedAt_=now; if (failures_) { failures_=0; nextAttemptAt_=now; } }
  }
  if (busy_) {
    FetchResult result{0,0,nullptr};
    const FetchState state=fetcher_.poll(requestId_,result);
    if (state==FetchState::Done) finish(result,now);
    else if (state==FetchState::Idle || now-attemptAt_>=WATCHDOG_MS) fail(WeatherError::Timeout,now);
    return;
  }
  if (!connected) return;
  // Give NTP a short head start so the cache gets a real time stamp; manual
  // refresh skips the wait (TLS chain checks do not depend on the date here).
  if (!time_.valid() && now-connectedAt_<CLOCK_GRACE_MS && !manual_) return;
  if (static_cast<int32_t>(now-nextAttemptAt_)>=0) { manual_=false; start(now); }
}

bool WeatherService::requestRefresh() {
  const uint32_t now=clock_.nowMs();
  if (busy_ || (attempted_ && now-attemptAt_<MANUAL_MIN_GAP_MS)) return false;
  manual_=true; nextAttemptAt_=now; ++revision_;
  return true;
}
bool WeatherService::setLocation(const WeatherLocation& l) {
  if (!validLocation(l)) return false;
  const WeatherLocationRecord record=makeLocationRecord(l);
  if (!store_.save(NS,"loc",&record,sizeof(record))) return false;
  location_=l; buildWeatherUrl(location_,url_,sizeof(url_));
  discardResult_=busy_;
  memset(&reading_,0,sizeof(reading_)); source_=WeatherSource::None;  // Old place's data is not shown.
  failures_=0; nextAttemptAt_=clock_.nowMs(); ++revision_;
  return true;
}
}  // namespace deskos
