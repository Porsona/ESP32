#include <cstdio>
#include <Arduino.h>
#include "apps/FocusQuestScreen.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
#define CHECK(expression) do { if (!(expression)) { std::printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expression); ++failures; return; } } while (0)
struct Clock : IFocusClock { uint32_t nowMs() const override { return testNow; } };
struct Storage : IFocusStorage {
  FocusRecord record{}; bool present=false;
  FocusLoadResult load(FocusRecord& out) override { out=record; return present?FocusLoadResult::Loaded:FocusLoadResult::Missing; }
  bool save(const FocusRecord& in) override { record=in; present=true; return true; }
};
struct Dummy : IApp {
  void onEnter() override { dirty_=true; }
  void onExit() override {}
  void update(uint32_t) override {}
  void render() override { dirty_=false; }
  void onTouch(const TouchEvent&) override {}
};
struct Fixture {
  Clock clock; Storage storage; ProgressService progress;
  FocusSessionService timer;
  DisplayHAL display; PixelRenderer renderer; AppManager manager; Dummy dummy;
  FocusQuestScreen screen;
  Fixture() : timer(clock,storage,progress),renderer(display),screen(renderer,manager,timer,progress) {
    testNow=0; timer.begin();
    for (uint8_t id=0;id<APP_COUNT;++id) manager.registerApp(static_cast<AppId>(id),
      id==static_cast<uint8_t>(AppId::Focus)?static_cast<IApp&>(screen):static_cast<IApp&>(dummy));
    manager.open(AppId::Focus); manager.update(0);
  }
  void event(TouchEvent::Type type,int16_t x,int16_t y,uint32_t held=10) {
    manager.onTouch({type,x,y,testNow,held});
  }
  void tap(int16_t x,int16_t y) {
    event(TouchEvent::Type::Press,x,y); event(TouchEvent::Type::Release,x,y);
    event(TouchEvent::Type::Tap,x,y); manager.update(0);
  }
  void tick(uint32_t ms) { testNow+=ms; timer.update(); manager.update(ms); }
};
void navigationKeepsTimer() {
  Fixture f; f.tap(70,170); CHECK(f.timer.state()==FocusState::Running);
  f.tick(10000); f.tap(240,220); CHECK(f.manager.activeId()==AppId::Home);
  f.tick(10000); CHECK(f.timer.remainingMs()==1480000);
  for (uint8_t id=1;id<APP_COUNT;++id) { f.manager.open(static_cast<AppId>(id)); f.manager.update(0); f.tick(1000); }
  f.manager.open(AppId::Focus); f.manager.update(0);
  CHECK(f.timer.remainingMs()==1474000 && f.timer.state()==FocusState::Running);
  f.tap(70,220); CHECK(f.manager.activeId()==AppId::Home); // BACK retains Home behavior.
  f.manager.suspend(); f.tick(1474000);
  CHECK(f.progress.totalXP()==25 && f.timer.mode()==FocusMode::Break);
}
void duplicateTapAndCancelledGestures() {
  Fixture f; f.tap(70,170);
  f.event(TouchEvent::Type::Tap,70,170); f.manager.update(0);
  CHECK(f.timer.state()==FocusState::Running); // Duplicate TAP cannot pause.
  f.event(TouchEvent::Type::Press,70,170);
  f.event(TouchEvent::Type::Move,90,170);
  f.event(TouchEvent::Type::Release,90,170);
  f.event(TouchEvent::Type::Tap,90,170);
  CHECK(f.timer.state()==FocusState::Running);
  f.event(TouchEvent::Type::Press,70,170);
  f.event(TouchEvent::Type::Release,70,170,701);
  f.event(TouchEvent::Type::Tap,70,170,701);
  CHECK(f.timer.state()==FocusState::Running);
  f.tap(70,170); CHECK(f.timer.state()==FocusState::Paused);
  f.tick(10000); CHECK(f.timer.remainingMs()==FOCUS_DURATION_MS);
  f.tap(70,170); CHECK(f.timer.state()==FocusState::Running);
}
void resetNeedsConfirm() {
  Fixture f; f.tap(70,170); f.tick(10000); f.tap(240,170);
  CHECK(f.timer.state()==FocusState::Running && f.timer.remainingMs()==1490000);
  f.tap(70,170); CHECK(f.timer.state()==FocusState::Running); // CANCEL.
  f.tap(240,170); f.tap(240,170);
  CHECK(f.timer.state()==FocusState::Idle && f.timer.remainingMs()==FOCUS_DURATION_MS);
  CHECK(f.progress.totalXP()==0);
  f.tap(70,170); f.tick(10000); f.tap(240,170); f.tick(5000);
  f.tap(240,170); CHECK(f.timer.state()==FocusState::Running); // Timeout requires a new confirmation.
  f.tap(240,170); CHECK(f.timer.state()==FocusState::Idle);
}
void phaseBoundaryDoesNotTurnPauseIntoStart() {
  Fixture f; f.tap(70,170); f.tick(FOCUS_DURATION_MS-1);
  f.event(TouchEvent::Type::Press,70,170);
  f.tick(1);
  f.event(TouchEvent::Type::Release,70,170);
  f.event(TouchEvent::Type::Tap,70,170); f.manager.update(0);
  CHECK(f.timer.mode()==FocusMode::Break && f.timer.state()==FocusState::Completed);
  CHECK(f.progress.totalXP()==25);
  f.tap(70,170); CHECK(f.timer.state()==FocusState::Running);
}
void dirtyRegionsAndBounds() {
  Fixture f; auto& draw=f.display.graphics();
  CHECK(draw.outside==0 && draw.clears==1);
  const unsigned before=draw.draws; f.manager.update(0);
  CHECK(draw.draws==before);
  f.tap(70,170); const unsigned clears=draw.clears;
  f.tick(1000); CHECK(draw.clears==clears && draw.draws>before && draw.outside==0);
  f.tap(240,170); f.tap(240,170); CHECK(draw.outside==0 && draw.clears==clears);
}
}
int main() {
  struct Test { const char* name; void(*run)(); };
  const Test tests[]={
    {"actual AppManager navigation/suspend retains core timer",navigationKeepsTimer},
    {"single TAP, drag and hold cancellation",duplicateTapAndCancelledGestures},
    {"reset confirm/cancel/timeout",resetNeedsConfirm},
    {"PRESS/TAP across completion boundary",phaseBoundaryDoesNotTurnPauseIntoStart},
    {"dirty regions, idle render and 320x240 draw bounds",dirtyRegionsAndBounds}
  };
  for (const auto& test:tests) { const int before=failures; test.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",test.name); }
  std::printf("RESULT: %u UI integration tests, %d failures (host draw spy, not hardware)\n",
              static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
