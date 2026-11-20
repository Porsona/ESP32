#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include "TestSupport.h"
#include "apps/HomeScreen.h"
#include "apps/FocusQuestScreen.h"
#include "apps/DailyMissionsScreen.h"
#include "apps/PixelPetScreen.h"
#include "apps/PixelWindowScreen.h"
#include "apps/WeatherScreen.h"
#include "apps/PcMonitorScreen.h"
#include "services/FocusCompletionAdapter.h"
#include "services/LiveWindowContext.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
struct FocusStore : IFocusStorage {
  FocusRecord r{}; bool present=false;
  FocusLoadResult load(FocusRecord& out) override { out=r; return present?FocusLoadResult::Loaded:FocusLoadResult::Missing; }
  bool save(const FocusRecord& in) override { r=in; present=true; return true; }
};
struct MissionStore : IMissionStorage {
  MissionRecord r{}; bool present=false;
  MissionLoadResult load(MissionRecord& out) override { out=r; return present?MissionLoadResult::Loaded:MissionLoadResult::Missing; }
  bool save(const MissionRecord& in) override { r=in; present=true; return true; }
};
struct Net : INetworkStatus {
  NetState s=NetState::Off;
  NetState state() const override { return s; }
  uint32_t ipv4() const override { return 0x3201A8C0UL; }
  uint32_t subnetMask() const override { return 0x00FFFFFFUL; }
  int8_t rssi() const override { return -60; }
};
struct Time : ITimeSource {
  bool valid() const override { return false; }  // Offline device: no clock, no guessing.
  uint32_t epoch() const override { return 0; }
};
struct Fetcher : IHttpFetcher {
  bool start(uint32_t,const char*) override { return false; }
  FetchState poll(uint32_t,FetchResult&) override { return FetchState::Idle; }
};
struct Random : IRandomSource { void fill(uint8_t* o,size_t n) override { for (size_t i=0;i<n;++i) o[i]=static_cast<uint8_t>(i+1); } };

struct Device {
  TestClock clock; FocusStore focusStore; MissionStore missionStore; FakeBlobStore blobs;
  Net net; Time time; Fetcher fetcher; Random random;
  ProgressService progress;
  FocusSessionService focus; FocusCompletionAdapter adapter; DailyMissionService missions;
  PetService pet; WeatherService weather; LiveWindowContext windowContext; PcMonitorService pc;
  DisplayHAL display; PixelRenderer renderer; AppManager manager;
  HomeScreen home; FocusQuestScreen focusScreen; DailyMissionsScreen missionsScreen; PixelPetScreen petScreen;
  PixelWindowScreen windowScreen; WeatherScreen weatherScreen; PcMonitorScreen pcScreen;
  Device()
      : focus(clock,focusStore,progress),adapter(focus,progress),missions(clock,missionStore,progress,adapter),
        pet(blobs,progress,clock),weather(clock,blobs,fetcher,net,time),windowContext(time,weather),
        pc(blobs,clock,time,random),renderer(display),home(renderer,manager),
        focusScreen(renderer,manager,focus,progress),missionsScreen(renderer,manager,missions,progress),
        petScreen(renderer,manager,pet),windowScreen(renderer,manager,blobs,windowContext),
        weatherScreen(renderer,manager,weather,net,time,clock),pcScreen(renderer,manager,pc,net,clock) {
    focus.begin(); missions.begin(); pet.begin(); windowScreen.begin(); weather.begin(); pc.begin();  // main.cpp order
    IApp* apps[APP_COUNT]={&home,&petScreen,&windowScreen,&focusScreen,&missionsScreen,&pcScreen,&weatherScreen};
    for (uint8_t id=0;id<APP_COUNT;++id) manager.registerApp(static_cast<AppId>(id),*apps[id]);
    manager.home(); manager.update(0);
  }
  void loop(uint32_t ms) {  // Same service order as main.cpp loop().
    clock.time+=ms; testNow=clock.time; focus.update(); missions.update(); weather.update(); manager.update(ms);
  }
  void tap(int16_t x,int16_t y) {
    const TouchEvent::Type seq[3]={TouchEvent::Type::Press,TouchEvent::Type::Release,TouchEvent::Type::Tap};
    for (auto t:seq) manager.onTouch({t,x,y,clock.time,10});
    loop(20);
  }
  void tapTile(AppId id) {
    const Rect& r=layout::Tiles[static_cast<uint8_t>(id)-1];
    tap(static_cast<int16_t>(r.x+r.w/2),static_cast<int16_t>(r.y+r.h/2));
  }
};

void everyAppFromHomeWithHomeAndBack() {
  Device d; CHECK(d.manager.activeId()==AppId::Home);
  const AppId apps[6]={AppId::Pet,AppId::Window,AppId::Focus,AppId::Missions,AppId::PcMonitor,AppId::Weather};
  for (AppId id:apps) {
    d.tapTile(id); CHECK(d.manager.activeId()==id);
    for (int i=0;i<50;++i) d.loop(20);
    d.tap(240,220); CHECK(d.manager.activeId()==AppId::Home);   // HOME
    d.tapTile(id); CHECK(d.manager.activeId()==id);
    d.tap(80,220); CHECK(d.manager.activeId()==AppId::Home);    // BACK
  }
  CHECK(d.display.graphics().outside==0);
}
void focusTimerAndSharedXpAcrossApps() {
  Device d;
  d.tapTile(AppId::Focus);
  const uint32_t startedAt=d.clock.time;  // START runs inside the tap, before its loop tick.
  d.tap(70,170); CHECK(d.focus.state()==FocusState::Running);
  const AppId tour[7]={AppId::Home,AppId::Pet,AppId::Window,AppId::Missions,AppId::Weather,AppId::PcMonitor,AppId::Home};
  for (int round=0;round<5;++round) for (AppId id:tour) {
    if (id==AppId::Home) d.tap(240,220); else d.tapTile(id);
    for (int i=0;i<100;++i) d.loop(100);
  }
  CHECK(d.focus.state()==FocusState::Running);
  CHECK(d.focus.remainingMs()==FOCUS_DURATION_MS-(d.clock.time-startedAt));  // Never paused/reset by switches.
  d.tapTile(AppId::Missions); d.tap(120,82);                   // Quest 1: +10
  CHECK(d.progress.totalXP()==10);
  d.tap(240,220); d.tapTile(AppId::Pet);
  for (int i=0;i<30000 && d.focus.state()==FocusState::Running;++i) d.loop(100);  // Focus ends on the Pet screen.
  CHECK(d.progress.focusXP()==25 && d.progress.bonusXP()==20 && d.progress.totalXP()==45);  // +25 Focus, +10 quest 5.
  CHECK(d.pet.xp()==45 && d.pet.level()==1);
  for (int i=0;i<100;++i) d.loop(100);
  CHECK(d.progress.totalXP()==45);                               // No duplicate rewards.
  d.tap(240,220); d.tapTile(AppId::Window);
  CHECK(d.windowScreen.shown()==WindowScene::Day);               // No clock: AUTO would not guess.
}
void steadyStateHasNoAllocations() {
  Device d;
  const unsigned long allocations=g_allocations;
  const AppId apps[6]={AppId::Pet,AppId::Window,AppId::Focus,AppId::Missions,AppId::PcMonitor,AppId::Weather};
  for (int round=0;round<20;++round) for (AppId id:apps) {
    d.tapTile(id); for (int i=0;i<40;++i) d.loop(25); d.tap(240,220);
  }
  CHECK(g_allocations==allocations && d.display.graphics().outside==0);
}
}  // namespace
int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"S01 all six apps open from Home tiles; HOME and BACK return",everyAppFromHomeWithHomeAndBack},
    {"S02 Focus timer survives app tour; XP shared Focus/Missions/Pet once",focusTimerAndSharedXpAcrossApps},
    {"S03 120 app switches with animation: zero heap allocations",steadyStateHasNoAllocations}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u shell integration tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
