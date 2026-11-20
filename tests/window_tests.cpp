#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include "TestSupport.h"
#include "apps/PixelWindowScreen.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
struct Dummy : IApp {
  void onEnter() override { dirty_=true; }
  void onExit() override {}
  void update(uint32_t) override {}
  void render() override { dirty_=false; }
  void onTouch(const TouchEvent&) override {}
};
struct FakeContext : IWindowContext {
  bool valid=false,rain=false; uint8_t hour=12;
  bool clockValid() const override { return valid; }
  uint8_t localHour() const override { return hour; }
  bool raining() const override { return rain; }
};
const int16_t GLASS_X=60,GLASS_Y=80,SCENE_X=284,SCENE_Y=75,SPEED_X=284,SPEED_Y=157,NAV_Y=220;

struct Fixture {
  FakeBlobStore store; FakeContext context;
  DisplayHAL display; PixelRenderer renderer; AppManager manager; Dummy dummy; PixelWindowScreen screen;
  explicit Fixture(const FakeBlobStore* existing=nullptr) : renderer(display),screen(renderer,manager,store,context) {
    if (existing) store=*existing;
    screen.begin();
    for (uint8_t id=0;id<APP_COUNT;++id)
      manager.registerApp(static_cast<AppId>(id),id==static_cast<uint8_t>(AppId::Window)?static_cast<IApp&>(screen):static_cast<IApp&>(dummy));
    manager.open(AppId::Home); manager.update(0);
  }
  void open() { manager.open(AppId::Window); manager.update(0); }
  void tap(int16_t x,int16_t y) {
    const TouchEvent::Type seq[3]={TouchEvent::Type::Press,TouchEvent::Type::Release,TouchEvent::Type::Tap};
    for (auto t:seq) manager.onTouch({t,x,y,0,10});
    manager.update(0);
  }
  void run(uint32_t ms,uint32_t tickMs=10) { for (uint32_t t=0;t<ms;t+=tickMs) manager.update(tickMs); }
  void forbidFrame() {
    auto& g=display.graphics(); g.clearForbidden();
    g.forbid(window::MULLION_V.x,window::MULLION_V.y,window::MULLION_V.w,window::MULLION_V.h);
    g.forbid(window::MULLION_H.x,window::MULLION_H.y,window::MULLION_H.w,window::MULLION_H.h);
    const Rect& f=window::FRAME;
    g.forbid(f.x,f.y,f.w,4); g.forbid(f.x,f.y+f.h-4,f.w,4); g.forbid(f.x,f.y,4,f.h); g.forbid(f.x+f.w-4,f.y,4,f.h);
    g.forbid(0,0,320,32); g.forbid(0,200,320,40);  // Status bar and navigation stay untouched.
  }
};

void settingsRecord() {
  WindowSettingsRecord r=defaultWindowSettings();
  CHECK(validWindowSettings(r) && r.scene==0 && r.speed==1);
  WindowSettingsRecord bad[4]={r,r,r,r};
  bad[0].checksum^=1; bad[1].scene=4; sealWindowSettings(bad[1]); bad[2].version=9; sealWindowSettings(bad[2]);
  bad[3].reserved[0]=1; sealWindowSettings(bad[3]);
  for (const auto& b:bad) {
    CHECK(!validWindowSettings(b));
    FakeBlobStore store; store.save("deskos-win","state",&b,sizeof(b));
    WindowSettingsRecord out; CHECK(loadWindowSettings(store,out)==BlobLoad::Invalid && out.scene==0 && out.speed==1);
  }
  FakeBlobStore io; io.readError=true; WindowSettingsRecord out;
  CHECK(loadWindowSettings(io,out)==BlobLoad::IoError && validWindowSettings(out));
}
void navigationScenesAndPersistence() {
  FakeBlobStore saved;
  {
    Fixture f; f.open(); CHECK(f.manager.activeId()==AppId::Window && f.screen.shown()==WindowScene::Day);
    f.tap(GLASS_X,GLASS_Y); CHECK(f.screen.selected()==WindowScene::Night && f.screen.shown()==WindowScene::Night);
    f.tap(SCENE_X,SCENE_Y); CHECK(f.screen.selected()==WindowScene::Rain && f.screen.shown()==WindowScene::Rain);
    f.tap(SPEED_X,SPEED_Y); CHECK(f.screen.speed()==WindowSpeed::Fast);
    f.tap(SPEED_X,SPEED_Y); CHECK(f.screen.speed()==WindowSpeed::Slow);
    CHECK(f.store.saves==0);  // Nothing written while browsing scenes.
    f.tap(240,NAV_Y); CHECK(f.manager.activeId()==AppId::Home && f.store.writes==1);
    f.open(); f.tap(80,NAV_Y); CHECK(f.manager.activeId()==AppId::Home && f.store.writes==1);  // Unchanged.
    saved=f.store;
  }
  Fixture g(&saved); CHECK(g.screen.loadResult()==BlobLoad::Loaded);
  g.open(); CHECK(g.screen.selected()==WindowScene::Rain && g.screen.speed()==WindowSpeed::Slow);
}
void autoNeverGuessesTime() {
  Fixture f; f.open();
  for (int i=0;i<3;++i) f.tap(SCENE_X,SCENE_Y);
  CHECK(f.screen.selected()==WindowScene::Auto && f.screen.shown()==WindowScene::Day);  // NO CLOCK fallback.
  f.context.valid=true; f.context.hour=22; f.run(50);
  CHECK(f.screen.shown()==WindowScene::Night);
  f.context.hour=6; f.run(50); CHECK(f.screen.shown()==WindowScene::Day);
  f.context.hour=17; f.run(50); CHECK(f.screen.shown()==WindowScene::Day);
  f.context.hour=18; f.run(50); CHECK(f.screen.shown()==WindowScene::Night);
  f.context.rain=true; f.run(50); CHECK(f.screen.shown()==WindowScene::Rain);
  f.context.valid=false; f.run(50); CHECK(f.screen.shown()==WindowScene::Day);
}
void speedTimingAndStalls() {
  Fixture f; f.open();
  uint32_t before=f.screen.steps(); f.run(7000); const uint32_t normal=f.screen.steps()-before;
  CHECK(normal>=99 && normal<=100);                      // 70 ms steps.
  f.tap(SPEED_X,SPEED_Y); before=f.screen.steps(); f.run(7000);
  const uint32_t fast=f.screen.steps()-before; CHECK(fast>=199 && fast<=200);  // 35 ms.
  f.tap(SPEED_X,SPEED_Y); before=f.screen.steps(); f.run(7000);
  const uint32_t slow=f.screen.steps()-before; CHECK(slow>=49 && slow<=50);    // 140 ms.
  before=f.screen.steps(); f.manager.update(10000); f.manager.update(0);
  CHECK(f.screen.steps()-before<=3);                     // A 10 s stall never bursts.
}
void animationIsClippedBoundedAndAllocationFree() {
  Fixture f; f.open(); f.tap(SPEED_X,SPEED_Y);  // FAST
  auto& g=f.display.graphics();
  const unsigned clears=g.clears;
  const unsigned long allocations=g_allocations;
  for (uint8_t scene=0;scene<3;++scene) {
    f.forbidFrame();
    for (int i=0;i<20000;++i) {
      f.manager.update(35);
      CHECK(f.screen.lastStepFills()<=300);  // Bounded SPI work per step.
    }
    CHECK(g.forbiddenHits==0 && g.outside==0);
    g.clearForbidden(); f.tap(GLASS_X,GLASS_Y);
  }
  CHECK(g.clears==clears && g_allocations==allocations);
  // Detector self-check: a full re-entry paints the frame, so it must register hits.
  f.forbidFrame(); f.open(); f.manager.open(AppId::Home); f.manager.update(0); f.open();
  CHECK(g.forbiddenHits>0);
}
}  // namespace
int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"W01 settings record validation, corrupt/IO fallback",settingsRecord},
    {"W02 open/HOME/BACK, tap + SCENE/SPEED, save on exit only",navigationScenesAndPersistence},
    {"W03 AUTO uses only a valid clock/weather context",autoNeverGuessesTime},
    {"W04 SLOW/NORMAL/FAST step timing, stall bound",speedTimingAndStalls},
    {"W05 DAY/NIGHT/RAIN 20k steps: clipped, bounded, no alloc",animationIsClippedBoundedAndAllocationFree}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u window tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
