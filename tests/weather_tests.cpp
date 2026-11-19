#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include "TestSupport.h"
#include "apps/WeatherScreen.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
// Captured from api.open-meteo.com on 2026-11-20 (Ho Chi Minh City).
const char SAMPLE[]=
  "{\"latitude\":10.790861,\"longitude\":106.6313,\"generationtime_ms\":0.17786026000976562,"
  "\"utc_offset_seconds\":25200,\"timezone\":\"Asia/Ho_Chi_Minh\",\"timezone_abbreviation\":\"GMT+7\","
  "\"elevation\":7.0,\"current_units\":{\"time\":\"iso8601\",\"interval\":\"seconds\",\"temperature_2m\":\"\xC2\xB0""C\","
  "\"relative_humidity_2m\":\"%\",\"weather_code\":\"wmo code\",\"wind_speed_10m\":\"km/h\",\"is_day\":\"\"},"
  "\"current\":{\"time\":\"2026-11-20T01:45\",\"interval\":900,\"temperature_2m\":26.3,"
  "\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":0}}";

struct FakeNet : INetworkStatus {
  NetState s=NetState::Connecting;
  NetState state() const override { return s; }
  uint32_t ipv4() const override { return 0x3201A8C0UL; }  // 192.168.1.50
  uint32_t subnetMask() const override { return 0x00FFFFFFUL; }
  int8_t rssi() const override { return -61; }
};
struct FakeTime : ITimeSource {  // Epoch advances with the fake millisecond clock.
  bool ok=false; uint32_t base=1763600000UL; const TestClock* clock=nullptr;
  bool valid() const override { return ok; }
  uint32_t epoch() const override { return base+(clock?clock->time/1000:0); }
};
struct FakeFetcher : IHttpFetcher {
  bool busy=false,done=false,hang=false; uint32_t id=0; unsigned starts=0;
  int code=200; const char* body=SAMPLE;
  void settle() { if (busy && !hang) { busy=false; done=true; } }  // The task finishes on its own.
  bool start(uint32_t requestId,const char*) override {
    settle();
    if (busy) return false;
    ++starts; id=requestId; busy=true; done=false; return true;
  }
  FetchState poll(uint32_t requestId,FetchResult& out) override {
    settle();
    if (busy) return FetchState::Busy;
    if (done && requestId==id) { done=false; out.httpCode=code; out.body=body; out.length=body?std::strlen(body):0; return FetchState::Done; }
    return FetchState::Idle;
  }
};
struct Fixture {
  TestClock clock; FakeBlobStore store; FakeFetcher fetcher; FakeNet net; FakeTime time; WeatherService weather;
  Fixture() : weather(clock,store,fetcher,net,time) { time.clock=&clock; weather.begin(); }
  void run(uint32_t ms,uint32_t tick=100) { for (uint32_t t=0;t<ms;t+=tick) { clock.time+=tick; weather.update(); } }
};

void parser() {
  WeatherReading r;
  CHECK(parseOpenMeteo(SAMPLE,std::strlen(SAMPLE),r));
  CHECK(r.temperatureDeci==263 && r.humidity==85 && r.code==0 && r.windDeci==82 && r.isDay==0 && r.valid==1);
  const char spaced[]="{ \"current\" : { \"is_day\" : 1 , \"weather_code\" : 95, \"wind_speed_10m\": 12,"
                      " \"relative_humidity_2m\": 100, \"temperature_2m\": -3.45 } }";
  CHECK(parseOpenMeteo(spaced,std::strlen(spaced),r) && r.temperatureDeci==-35 && r.code==95 && r.isDay==1 && r.windDeci==120);
  const char* bad[]={
    "{\"current_units\":{\"temperature_2m\":\"C\"}}",                                      // Units only.
    "{\"current\":{\"temperature_2m\":26.3,\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8.2}}",  // No is_day.
    "{\"current\":{\"temperature_2m\":null,\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":1}}",
    "{\"current\":{\"temperature_2m\":99.0,\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":1}}",
    "{\"current\":{\"temperature_2m\":20,\"relative_humidity_2m\":185,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":1}}",
    "{\"current\":{\"temperature_2m\":20,\"relative_humidity_2m\":85.5,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":1}}",
    "{\"current\":{\"temperature_2m\":20,\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8.2,\"is_day\":2}}",
    "{\"current\":{\"temperature_2m\":20,\"relative_humidity_2m\":85,\"weather_code\":0,\"wind_speed_10m\":8",  // Truncated.
    "<html>502 Bad Gateway</html>",""
  };
  for (const char* b:bad) CHECK(!parseOpenMeteo(b,std::strlen(b),r));
  CHECK(!parseOpenMeteo(nullptr,10,r));
  CHECK(!parseOpenMeteo(SAMPLE,std::strlen(SAMPLE)-3,r));  // Cut inside "current".
  CHECK(weatherKind(0)==WeatherKind::Clear && weatherKind(2)==WeatherKind::PartlyCloudy && weatherKind(45)==WeatherKind::Fog);
  CHECK(weatherKind(63)==WeatherKind::Rain && weatherKind(81)==WeatherKind::Rain && weatherKind(53)==WeatherKind::Drizzle);
  CHECK(weatherKind(75)==WeatherKind::Snow && weatherKind(99)==WeatherKind::Storm && weatherKind(42)==WeatherKind::Unknown);
  CHECK(wetWeather(WeatherKind::Storm) && !wetWeather(WeatherKind::Fog));
  for (int c=0;c<100;++c) CHECK(std::strlen(weatherText(static_cast<uint8_t>(c)))<=13);  // Fits size-2 GLCD line.
  CHECK(weatherSpritesWellFormed());
}
void locationTimeAndRecords() {
  int32_t v=0;
  CHECK(parseCoordinateE4("10.8231",900000,v) && v==108231);
  CHECK(parseCoordinateE4("-33.86",900000,v) && v==-338600 && parseCoordinateE4("106.62971",1800000,v) && v==1066297);
  CHECK(!parseCoordinateE4("91",900000,v) && !parseCoordinateE4("10.5x",900000,v) && !parseCoordinateE4("",900000,v));
  char url[256];
  CHECK(buildWeatherUrl(defaultLocation(),url,sizeof(url)));
  CHECK(!std::strcmp(url,"https://api.open-meteo.com/v1/forecast?latitude=10.8231&longitude=106.6297"
                         "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
                         "&wind_speed_unit=kmh&timezone=Asia%2FHo_Chi_Minh"));
  CHECK(!buildWeatherUrl(defaultLocation(),url,40));
  WeatherLocation l=defaultLocation(); l.label[0]='a'; CHECK(!validLocation(l));
  // ICT day boundary: 2026-11-19 16:59:59Z is 23:59:59 local, 17:00:00Z is the next local day.
  const uint32_t before=1763571599UL,after=1763571600UL;
  CHECK(localDayKey(after)==localDayKey(before)+1 && localHour(before)==23 && localHour(after)==0);
  CHECK(localMinute(before)==59 && plausibleEpoch(after) && !plausibleEpoch(86400));
  WeatherReading r; parseOpenMeteo(SAMPLE,std::strlen(SAMPLE),r);
  WeatherCacheRecord c=makeWeatherCache(r,defaultLocation(),after);
  CHECK(validWeatherCache(c));
  c.reading.humidity=101; sealWeatherCache(c); CHECK(!validWeatherCache(c));
  c=makeWeatherCache(r,defaultLocation(),after); c.checksum^=1; CHECK(!validWeatherCache(c));
  CHECK(validLocationRecord(makeLocationRecord(defaultLocation())));
}
void schedulingSuccessAndCache() {
  Fixture f;
  f.run(60000); CHECK(f.fetcher.starts==0 && !f.weather.hasReading());   // Offline: no attempts.
  f.net.s=NetState::Connected; f.run(19900);
  CHECK(f.fetcher.starts==0);                                            // NTP grace period.
  f.run(300); CHECK(f.fetcher.starts==1 && f.weather.hasReading() && f.weather.source()==WeatherSource::Live);
  CHECK(f.store.writes==1 && f.weather.lastError()==WeatherError::None);  // One cache write.
  f.run(WeatherService::REFRESH_MS-1000);
  CHECK(f.fetcher.starts==1 && f.store.writes==1);
  f.run(1100); CHECK(f.fetcher.starts==2 && f.store.writes==2);
  uint32_t age=99; CHECK(f.weather.ageSeconds(age) && age==0 && !f.weather.stale());
  // Clock valid: no grace wait, cache carries an epoch so age survives a reboot.
  f.time.ok=true; f.run(40000); CHECK(f.weather.requestRefresh()); f.run(300);
  FakeBlobStore saved=f.store;
  TestClock clock; FakeFetcher fetcher; FakeNet net; FakeTime time; time.ok=true; time.base=f.time.epoch()+7200;
  net.s=NetState::Off;
  WeatherService reboot(clock,saved,fetcher,net,time); reboot.begin();
  CHECK(reboot.hasReading() && reboot.source()==WeatherSource::Cache && reboot.ageSeconds(age));
  CHECK(age>=7200 && age<7300 && !reboot.stale());
  time.base+=2*3600; CHECK(reboot.stale() && !reboot.raining());
  time.ok=false; CHECK(!reboot.ageSeconds(age) && reboot.stale());       // Unknown age is never "current".
}
void failuresTimeoutsAndBackoff() {
  Fixture f; f.net.s=NetState::Connected; f.time.ok=true;
  f.fetcher.code=500; f.run(200); CHECK(f.fetcher.starts==1 && f.weather.lastError()==WeatherError::Http);
  f.run(29800); CHECK(f.fetcher.starts==1);
  f.fetcher.code=-1; f.run(300); CHECK(f.fetcher.starts==2 && f.weather.lastError()==WeatherError::Transport);
  f.run(59900); CHECK(f.fetcher.starts==2);
  f.fetcher.code=200; f.fetcher.body="{\"oops\":1}"; f.run(300);
  CHECK(f.fetcher.starts==3 && f.weather.lastError()==WeatherError::Parse && f.weather.failures()==3);
  CHECK(!f.weather.hasReading() && f.store.writes==0);
  // Hung request: watchdog frees the UI state; a late result for that id is ignored.
  f.fetcher.body=SAMPLE; f.fetcher.hang=true; f.run(120000+500);
  CHECK(f.fetcher.starts==4 && f.weather.fetching());
  f.run(WeatherService::WATCHDOG_MS);
  CHECK(!f.weather.fetching() && f.weather.lastError()==WeatherError::Timeout && f.weather.failures()==4);
  f.run(240000); CHECK(f.weather.lastError()==WeatherError::Busy && f.fetcher.starts==4);  // Task still busy.
  f.fetcher.hang=false; f.run(6000);
  CHECK(f.fetcher.starts==5 && f.weather.hasReading() && f.weather.failures()==0);
  // Backoff grows 30/60/120/240/480 s, then caps at 10 minutes.
  f.fetcher.code=503;
  f.run(WeatherService::REFRESH_MS+200);
  f.run((30+60+120+240+480)*1000UL+2000);
  unsigned last=f.fetcher.starts;
  for (int guard=0;guard<7000 && f.fetcher.starts==last;++guard) f.run(100);  // Align to an attempt.
  f.run(200); last=f.fetcher.starts;
  f.run(WeatherService::RETRY_MAX_MS-1500); CHECK(f.fetcher.starts==last);
  f.run(2000); CHECK(f.fetcher.starts==last+1 && f.weather.failures()>=7);
  CHECK(f.weather.msUntilNextAttempt()<=WeatherService::RETRY_MAX_MS && f.weather.hasReading());  // Old data kept.
  // Reconnect resets the backoff and retries promptly.
  f.net.s=NetState::Connecting; f.run(1000); f.net.s=NetState::Connected; f.fetcher.code=200;
  last=f.fetcher.starts; f.run(300); CHECK(f.fetcher.starts==last+1 && f.weather.failures()==0);
}
void manualRefreshAndLocation() {
  Fixture f; f.net.s=NetState::Connected;
  CHECK(f.weather.requestRefresh()); f.run(100); CHECK(f.fetcher.starts==1);   // Manual skips NTP grace.
  CHECK(!f.weather.requestRefresh()); f.run(29000); CHECK(!f.weather.requestRefresh());
  f.run(1000); CHECK(f.weather.requestRefresh()); f.run(100); CHECK(f.fetcher.starts==2);
  WeatherLocation hanoi=defaultLocation(); hanoi.latE4=210285; hanoi.lonE4=1058542;
  std::memset(hanoi.label,0,sizeof(hanoi.label)); std::snprintf(hanoi.label,sizeof(hanoi.label),"%s","HA NOI");
  WeatherLocation dirty=hanoi; std::memcpy(dirty.label+7,"XX",2); CHECK(!validLocation(dirty));  // Padding must be zero.
  CHECK(f.weather.setLocation(hanoi) && !f.weather.hasReading());
  f.run(100); CHECK(!f.weather.hasReading());  // In-flight result for the old place is discarded.
  f.run(300); CHECK(f.fetcher.starts==3 && f.weather.hasReading());
  WeatherLocation bad=hanoi; bad.latE4=910000; CHECK(!f.weather.setLocation(bad));
  FakeBlobStore saved=f.store; TestClock c; FakeFetcher fe; FakeNet n; FakeTime t;
  WeatherService reboot(c,saved,fe,n,t); reboot.begin();
  CHECK(!std::strcmp(reboot.location().label,"HA NOI") && reboot.hasReading());
  f.fetcher.body="{\"current\":{\"temperature_2m\":24,\"relative_humidity_2m\":90,\"weather_code\":63,\"wind_speed_10m\":5,\"is_day\":1}}";
  f.run(WeatherService::REFRESH_MS+200); CHECK(f.weather.raining());
}
struct Dummy : IApp {
  void onEnter() override { dirty_=true; }
  void onExit() override {}
  void update(uint32_t) override {}
  void render() override { dirty_=false; }
  void onTouch(const TouchEvent&) override {}
};
void screenNavigationBoundsAllocations() {
  Fixture f; DisplayHAL display; PixelRenderer renderer(display); AppManager manager; Dummy dummy;
  WeatherScreen screen(renderer,manager,f.weather,f.net,f.time,f.clock);
  for (uint8_t id=0;id<APP_COUNT;++id)
    manager.registerApp(static_cast<AppId>(id),id==static_cast<uint8_t>(AppId::Weather)?static_cast<IApp&>(screen):static_cast<IApp&>(dummy));
  manager.open(AppId::Weather); manager.update(0);
  auto& g=display.graphics();
  CHECK(manager.activeId()==AppId::Weather && g.clears==1 && g.outside==0);
  auto tap=[&](int16_t x,int16_t y) {
    const TouchEvent::Type seq[3]={TouchEvent::Type::Press,TouchEvent::Type::Release,TouchEvent::Type::Tap};
    for (auto t:seq) manager.onTouch({t,x,y,0,10});
    manager.update(0);
  };
  tap(254,178); CHECK(f.fetcher.starts==0);  // Offline: REFRESH explains, never blocks.
  const unsigned long allocations=g_allocations;
  f.net.s=NetState::Connected; f.time.ok=true;
  for (int i=0;i<4000;++i) {
    f.clock.time+=50; f.weather.update(); manager.update(50);
    if (i==500) f.fetcher.code=500;
    if (i==1500) { f.fetcher.code=200; f.fetcher.body="{\"current\":{\"temperature_2m\":-12.3,\"relative_humidity_2m\":100,\"weather_code\":82,\"wind_speed_10m\":123.4,\"is_day\":1}}"; }
    if (i==2500) f.net.s=NetState::NoConfig;
    if (i==3000) f.net.s=NetState::Connected;
    if (i%700==0) tap(254,178);
  }
  CHECK(f.fetcher.starts>=2 && g.outside==0 && g.clears==1 && g_allocations==allocations);
  tap(240,220); CHECK(manager.activeId()==AppId::Home);
  manager.open(AppId::Weather); manager.update(0); tap(80,220); CHECK(manager.activeId()==AppId::Home);
}
}  // namespace
int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"WE1 Open-Meteo parser (real sample), bad/truncated bodies, WMO map",parser},
    {"WE2 coordinates, URL, ICT day boundary, cache/location records",locationTimeAndRecords},
    {"WE3 offline/NTP grace/15-min schedule/cache restore/stale",schedulingSuccessAndCache},
    {"WE4 HTTP/TLS/parse errors, watchdog timeout, backoff cap, reconnect",failuresTimeoutsAndBackoff},
    {"WE5 manual refresh rate limit, location change, raining()",manualRefreshAndLocation},
    {"WE6 screen: offline REFRESH, states, bounds, no allocations",screenNavigationBoundsAllocations}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u weather tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
