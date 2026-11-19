#include <cstdio>
#include <Arduino.h>
#include "apps/FocusQuestScreen.h"
#include "apps/DailyMissionsScreen.h"
#include "services/FocusCompletionAdapter.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
#define CHECK(expression) do { if (!(expression)) { std::printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expression); ++failures; return; } } while (0)
struct Clock : IFocusClock { uint32_t nowMs() const override { return testNow; } };
struct FocusStore : IFocusStorage {
  FocusRecord record{}; bool present=false; unsigned writes=0;
  FocusLoadResult load(FocusRecord& out) override { out=record; return present?FocusLoadResult::Loaded:FocusLoadResult::Missing; }
  bool save(const FocusRecord& in) override { record=in; present=true; ++writes; return true; }
};
struct MissionStore : IMissionStorage {
  MissionRecord record{}; bool present=false; unsigned writes=0;
  MissionLoadResult load(MissionRecord& out) override { out=record; return present?MissionLoadResult::Loaded:MissionLoadResult::Missing; }
  bool save(const MissionRecord& in) override { record=in; present=true; ++writes; return true; }
};
struct Dummy : IApp {
  void onEnter() override { dirty_=true; }
  void onExit() override {}
  void update(uint32_t) override {}
  void render() override { dirty_=false; }
  void onTouch(const TouchEvent&) override {}
};
// Screen coordinates (centres) from DailyMissionsScreen's layout.
const int16_t ROW_X=120,ROW_Y[3]={82,128,174},SIDE_X=285,PREV_Y=82,NEXT_Y=128,NEWDAY_Y=174;
const int16_t CONFIRM_X=84,CANCEL_X=236,DIALOG_BTN_Y=168,NAV_Y=220,BACK_X=70,HOME_X=240;

struct Fixture {
  Clock clock; FocusStore focusStore; MissionStore missionStore; ProgressService progress;
  FocusSessionService timer; FocusCompletionAdapter adapter; DailyMissionService missions;
  DisplayHAL display; PixelRenderer renderer; AppManager manager; Dummy dummy;
  FocusQuestScreen focusScreen; DailyMissionsScreen screen;
  Fixture() : timer(clock,focusStore,progress),adapter(timer,progress),missions(clock,missionStore,progress,adapter),
              renderer(display),focusScreen(renderer,manager,timer,progress),
              screen(renderer,manager,missions,progress) {
    testNow=1000; timer.begin(); missions.begin();
    for (uint8_t id=0;id<APP_COUNT;++id) {
      IApp* app=&dummy;
      if (id==static_cast<uint8_t>(AppId::Focus)) app=&focusScreen;
      if (id==static_cast<uint8_t>(AppId::Missions)) app=&screen;
      manager.registerApp(static_cast<AppId>(id),*app);
    }
    open(AppId::Missions);
  }
  void open(AppId id) { manager.open(id); manager.update(0); }
  void event(TouchEvent::Type type,int16_t x,int16_t y,uint32_t held=10) { manager.onTouch({type,x,y,testNow,held}); }
  void tap(int16_t x,int16_t y) {
    event(TouchEvent::Type::Press,x,y); event(TouchEvent::Type::Release,x,y);
    event(TouchEvent::Type::Tap,x,y); manager.update(0);
  }
  void tick(uint32_t ms) { testNow+=ms; timer.update(); missions.update(); manager.update(ms); }
};

void singleTapCompletes() {
  Fixture f; CHECK(f.manager.activeId()==AppId::Missions && f.missions.dayId()==1);
  f.tap(ROW_X,ROW_Y[0]);
  CHECK(questCompleted(f.missions.quest(0)) && f.progress.totalXP()==10);
  const unsigned writes=f.missionStore.writes;
  f.event(TouchEvent::Type::Tap,ROW_X,ROW_Y[0]); f.manager.update(0);  // Duplicate TAP.
  f.tap(ROW_X,ROW_Y[0]); f.tap(ROW_X,ROW_Y[0]);                     // Completed row: view only.
  CHECK(f.progress.totalXP()==10 && f.missionStore.writes==writes && questCompleted(f.missions.quest(0)));
  f.event(TouchEvent::Type::Press,ROW_X,ROW_Y[1]);                   // Drag off -> cancelled.
  f.event(TouchEvent::Type::Move,ROW_X,ROW_Y[1]+30);
  f.event(TouchEvent::Type::Release,ROW_X,ROW_Y[1]+30);
  f.event(TouchEvent::Type::Tap,ROW_X,ROW_Y[1]+30); f.manager.update(0);
  f.event(TouchEvent::Type::Press,ROW_X,ROW_Y[1]);                   // Hold -> cancelled.
  f.event(TouchEvent::Type::Release,ROW_X,ROW_Y[1],701);
  f.event(TouchEvent::Type::Tap,ROW_X,ROW_Y[1],701); f.manager.update(0);
  CHECK(!questCompleted(f.missions.quest(1)) && f.progress.totalXP()==10);
  f.tap(ROW_X,ROW_Y[1]); CHECK(questCompleted(f.missions.quest(1)) && f.progress.totalXP()==20);
  f.tick(3000); CHECK(f.missions.progressPermille()==400);
}
void pagesKeepState() {  // T13
  Fixture f; f.tap(ROW_X,ROW_Y[0]); CHECK(f.screen.page()==0);
  f.tap(SIDE_X,PREV_Y); CHECK(f.screen.page()==0);                  // PREV disabled on page 1.
  f.tap(SIDE_X,NEXT_Y); CHECK(f.screen.page()==1);
  f.tap(ROW_X,ROW_Y[0]);                                              // Slot 4 = REVIEW NOTES.
  CHECK(questCompleted(f.missions.quest(3)) && f.progress.totalXP()==20);
  f.tap(ROW_X,ROW_Y[1]);                                              // COMPLETE FOCUS is Focus-linked.
  CHECK(!questCompleted(f.missions.quest(4)) && f.progress.totalXP()==20);
  f.tap(ROW_X,ROW_Y[2]); f.tap(SIDE_X,NEXT_Y); CHECK(f.screen.page()==1);  // Empty row / last page.
  f.tap(SIDE_X,PREV_Y); CHECK(f.screen.page()==0 && questCompleted(f.missions.quest(0)));
  CHECK(!questCompleted(f.missions.quest(1)) && f.missions.completedCount()==2);
  f.tap(HOME_X,NAV_Y); CHECK(f.manager.activeId()==AppId::Home);
  f.open(AppId::Missions); CHECK(f.missions.completedCount()==2 && f.progress.totalXP()==20);
  f.tap(BACK_X,NAV_Y); CHECK(f.manager.activeId()==AppId::Home);
  const unsigned writes=f.missionStore.writes;
  for (int i=0;i<20;++i) { f.open(AppId::Missions); f.tap(SIDE_X,NEXT_Y); f.tap(SIDE_X,PREV_Y); f.open(AppId::Home); }
  CHECK(f.missionStore.writes==writes && f.progress.totalXP()==20);  // Open/page != reward/write.
}
void newDayConfirmation() {
  Fixture f; f.tap(ROW_X,ROW_Y[0]); f.tap(ROW_X,ROW_Y[1]); CHECK(f.progress.totalXP()==20);
  f.tap(SIDE_X,NEWDAY_Y); CHECK(f.screen.confirming() && f.missions.dayId()==1);
  f.tap(SIDE_X,NEWDAY_Y); CHECK(!f.screen.confirming() && f.missions.dayId()==1);  // 2nd tap = CANCEL.
  f.tap(SIDE_X,NEWDAY_Y); CHECK(!f.screen.confirming());  // 3rd quick tap: post-dialog guard.
  f.tick(500); f.tap(SIDE_X,NEWDAY_Y); f.tap(CONFIRM_X,DIALOG_BTN_Y);
  CHECK(f.screen.confirming() && f.missions.dayId()==1);  // Inside the 300 ms arm delay: ignored.
  f.tick(300); f.tap(CONFIRM_X,DIALOG_BTN_Y);
  CHECK(!f.screen.confirming() && f.missions.dayId()==2 && f.missions.completedCount()==0);
  CHECK(f.progress.totalXP()==20);
  f.tap(CONFIRM_X,DIALOG_BTN_Y);  // Extra tap where CONFIRM was lands on row 3: guarded.
  CHECK(f.missions.completedCount()==0 && f.progress.totalXP()==20);
  f.tick(500); f.tap(SIDE_X,NEWDAY_Y);  // Cooldown: no prompt, no second day.
  CHECK(!f.screen.confirming() && f.missions.dayId()==2 && f.missions.completedCount()==0);
  f.tick(2500); f.tap(SIDE_X,NEWDAY_Y); CHECK(f.screen.confirming());
  f.tick(5000); CHECK(!f.screen.confirming() && f.missions.dayId()==2);  // Timeout cancels.
  f.tick(500); f.tap(SIDE_X,NEWDAY_Y); CHECK(f.screen.confirming());
  f.tick(400); f.tap(HOME_X,NAV_Y);
  CHECK(f.manager.activeId()==AppId::Home && f.missions.dayId()==2);  // Leaving cancels too.
  f.open(AppId::Missions); CHECK(!f.screen.confirming());
  f.tap(ROW_X,ROW_Y[0]); CHECK(f.progress.totalXP()==30);           // mission:2:1 is new.
  // PRESS on a row, then the day changes before TAP: the stale gesture is void.
  f.event(TouchEvent::Type::Press,ROW_X,ROW_Y[1]);
  CHECK(f.missions.startNewDay(2)==DayResult::Started);
  f.event(TouchEvent::Type::Release,ROW_X,ROW_Y[1]); f.event(TouchEvent::Type::Tap,ROW_X,ROW_Y[1]);
  f.manager.update(0); CHECK(f.missions.completedCount()==0 && f.progress.totalXP()==30);
}
void focusTimerUnaffected() {  // T14 (UI level) + FocusCompleted while on Missions
  Fixture f; f.open(AppId::Focus); f.tap(70,170); CHECK(f.timer.state()==FocusState::Running);
  f.tick(10000); f.open(AppId::Missions);
  f.tap(ROW_X,ROW_Y[0]); f.tap(SIDE_X,NEXT_Y); f.tap(ROW_X,ROW_Y[0]); f.tap(SIDE_X,PREV_Y);
  f.tap(SIDE_X,NEWDAY_Y); f.tick(500); f.tap(CONFIRM_X,DIALOG_BTN_Y);
  CHECK(f.missions.dayId()==2 && f.progress.totalXP()==20);
  CHECK(f.timer.state()==FocusState::Running && f.timer.remainingMs()==FOCUS_DURATION_MS-10500);
  CHECK(f.focusStore.writes==1);
  f.tick(FOCUS_DURATION_MS-10500);  // Focus ends while Missions is on screen.
  CHECK(f.timer.mode()==FocusMode::Break && questCompleted(f.missions.quest(4)));
  CHECK(f.progress.focusXP()==25 && f.progress.bonusXP()==30 && f.progress.totalXP()==55);
  for (int i=0;i<50;++i) f.tick(100);
  CHECK(f.progress.totalXP()==55);
  f.open(AppId::Focus); f.tap(70,170); CHECK(f.timer.state()==FocusState::Running && f.timer.mode()==FocusMode::Break);
}
void dirtyRegionsAndBounds() {
  Fixture f; auto& draw=f.display.graphics();
  CHECK(draw.outside==0 && draw.clears==1);
  unsigned before=draw.draws; f.manager.update(0); f.tick(100);
  CHECK(draw.draws==before);                     // Idle: no redraw, no framebuffer.
  const unsigned clears=draw.clears;
  f.tap(ROW_X,ROW_Y[0]);                          // Completion + pixel effect frames.
  for (int i=0;i<10;++i) f.tick(100);
  f.tap(SIDE_X,NEXT_Y); f.tap(SIDE_X,PREV_Y);
  f.tap(SIDE_X,NEWDAY_Y); f.tick(400); f.tap(CANCEL_X,DIALOG_BTN_Y);
  f.tap(SIDE_X,NEWDAY_Y); f.tick(400); f.tap(CONFIRM_X,DIALOG_BTN_Y);
  f.tick(2500);
  CHECK(draw.clears==clears && draw.outside==0);  // Regional redraw only, inside 320x240.
  before=draw.draws; f.tick(100); f.tick(100); CHECK(draw.draws==before);
  f.open(AppId::Home); f.open(AppId::Missions); CHECK(draw.clears==clears+1 && draw.outside==0);  // Home is a dummy here.
}
}  // namespace
int main() {
  struct Test { const char* name; void(*run)(); };
  const Test tests[]={
    {"MU1 one TAP completes once; duplicate/drag/hold rejected",singleTapCompletes},
    {"MU2 pages/HOME/BACK keep state, no reward or write on open",pagesKeepState},
    {"MU3 NEW DAY confirm/cancel/arm/cooldown/timeout/stale press",newDayConfirmation},
    {"MU4 Focus timer unaffected; FocusCompleted while on Missions",focusTimerUnaffected},
    {"MU5 dirty regions, idle render and 320x240 bounds",dirtyRegionsAndBounds}
  };
  for (const auto& test:tests) { const int before=failures; test.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",test.name); }
  std::printf("RESULT: %u Missions UI integration tests, %d failures (host draw spy, not hardware)\n",
              static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
