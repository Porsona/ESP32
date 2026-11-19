#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include "TestSupport.h"
#include "apps/PixelPetScreen.h"

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
const int16_t PET_X=160,PET_Y=116,STYLE_X=274,STYLE_Y=85,SLEEP_X=274,SLEEP_Y=137,NAV_Y=220;

struct Fixture {
  TestClock clock; FakeBlobStore store; ProgressService progress; PetService pet;
  DisplayHAL display; PixelRenderer renderer; AppManager manager; Dummy dummy; PixelPetScreen screen;
  Fixture() : pet(store,progress,clock),renderer(display),screen(renderer,manager,pet) {
    pet.begin();
    for (uint8_t id=0;id<APP_COUNT;++id)
      manager.registerApp(static_cast<AppId>(id),id==static_cast<uint8_t>(AppId::Pet)?static_cast<IApp&>(screen):static_cast<IApp&>(dummy));
    manager.open(AppId::Home); manager.update(0);
  }
  void open() { manager.open(AppId::Pet); manager.update(0); }
  void tap(int16_t x,int16_t y) {
    const TouchEvent::Type seq[3]={TouchEvent::Type::Press,TouchEvent::Type::Release,TouchEvent::Type::Tap};
    for (auto t:seq) manager.onTouch({t,x,y,clock.time,10});
    manager.update(0);
  }
  void tick(uint32_t ms) { clock.time+=ms; manager.update(ms); }
  void setXP(uint32_t focus,uint32_t bonus=0) {
    ProgressTestAccess::setFocusXP(progress,focus); ProgressTestAccess::setBonusXP(progress,bonus);
  }
};

void levelMath() {
  CHECK(petLevel(0)==1 && petLevel(99)==1 && petLevel(100)==2 && petLevel(299)==2 && petLevel(300)==3);
  CHECK(petLevel(1000)==5 && petLevel(2100)==7 && petLevel(4500)==10 && petLevel(UINT32_MAX)==PET_MAX_LEVEL);
  CHECK(levelStartXP(1)==0 && levelStartXP(2)==100 && levelStartXP(10)==4500 && levelStartXP(200)==levelStartXP(99));
  CHECK(levelPermille(0)==0 && levelPermille(50)==500 && levelPermille(200)==500 && levelPermille(UINT32_MAX)==1000);
  CHECK(levelStartXP(PET_MAX_LEVEL)==485100 && levelPermille(485100)==1000);
  for (uint32_t xp=0;xp+37<levelStartXP(PET_MAX_LEVEL);xp+=37) {  // Monotonic, bar 0..999 below max.
    CHECK(petLevel(xp)<=petLevel(xp+37) && levelPermille(xp)<1000 && levelStartXP(petLevel(xp))<=xp);
  }
  CHECK(styleUnlocked(PetStyle::Classic,1) && !styleUnlocked(PetStyle::StarEyes,2) && styleUnlocked(PetStyle::StarEyes,3));
  CHECK(styleUnlockedAt(5)==PetStyle::HeartEyes && styleUnlockedAt(4)==PetStyle::Count && styleUnlockedAt(10)==PetStyle::Aura);
  CHECK(petSpritesWellFormed());
}
void moodsAndXpNeverChanges() {
  Fixture f; f.setXP(50,20);  // Shared XP = 70: Focus + Missions.
  f.open();
  CHECK(f.pet.xp()==70 && f.pet.level()==1 && f.pet.mood()==PetMood::Happy && f.pet.event()==PetEvent::XpGained);
  CHECK(f.pet.eventValue()==70);
  f.tick(PetService::HAPPY_MS); CHECK(f.pet.mood()==PetMood::Idle);
  for (int i=0;i<200;++i) f.tap(PET_X,PET_Y);
  CHECK(f.pet.petCount()==200 && f.progress.totalXP()==70);  // Pet never grants XP.
  CHECK(f.pet.mood()==PetMood::Happy);
  f.tick(PetService::HAPPY_MS); f.tick(PetService::SLEEP_AFTER_MS);
  CHECK(f.pet.mood()==PetMood::Sleep);
  f.tap(PET_X,PET_Y); CHECK(f.pet.mood()==PetMood::Happy && f.pet.event()==PetEvent::Woke);
  f.tap(SLEEP_X,SLEEP_Y); CHECK(f.pet.mood()==PetMood::Sleep);
  f.tap(SLEEP_X,SLEEP_Y); CHECK(f.pet.mood()==PetMood::Idle);
  // Live XP while visible (e.g. Focus completes): +25 -> LV2 at 100 XP celebrates.
  f.setXP(80,20); f.tick(10);
  CHECK(f.pet.level()==2 && f.pet.mood()==PetMood::Celebrate && f.pet.event()==PetEvent::LevelUp);
  f.tick(PetService::CELEBRATE_MS); CHECK(f.pet.mood()==PetMood::Idle);
}
void unlocksAndStyles() {
  Fixture f; f.open();
  f.tap(STYLE_X,STYLE_Y);
  CHECK(f.pet.style()==PetStyle::Classic && f.pet.event()==PetEvent::StyleLocked && f.pet.eventValue()==3);
  f.tap(80,220); f.setXP(300); f.open();  // Away, then back at LV3.
  CHECK(f.pet.event()==PetEvent::StyleUnlocked && f.pet.eventValue()==static_cast<uint8_t>(PetStyle::StarEyes));
  const unsigned writes=f.store.writes;
  f.tap(STYLE_X,STYLE_Y); CHECK(f.pet.style()==PetStyle::StarEyes && f.store.writes==writes+1);
  f.tap(STYLE_X,STYLE_Y); CHECK(f.pet.style()==PetStyle::Classic);
  f.setXP(4500); f.tick(10);
  CHECK(f.pet.level()==10 && f.pet.event()==PetEvent::StyleUnlocked &&
        f.pet.eventValue()==static_cast<uint8_t>(PetStyle::Aura));
  uint8_t seen=0;
  for (int i=0;i<5;++i) { f.tap(STYLE_X,STYLE_Y); seen|=1U<<static_cast<uint8_t>(f.pet.style()); }
  CHECK(seen==0x1F);  // All five styles reachable at LV10.
  // Frames: crown head for Crown/Aura, sleepy eyes in sleep, blink in idle.
  CHECK(petFrame(PetMood::Idle,PetStyle::Crown,0,0).head==PetHead::Crown);
  CHECK(petFrame(PetMood::Sleep,PetStyle::Classic,0,0).eyes==PetEyes::Sleepy);
  CHECK(petFrame(PetMood::Idle,PetStyle::Classic,3100,0).eyes==PetEyes::Closed);
  CHECK(petFrame(PetMood::Happy,PetStyle::HeartEyes,0,0).eyes==PetEyes::Heart);
}
void persistenceAndCorruption() {
  FakeBlobStore store;
  {
    Fixture f; f.setXP(120); f.open();
    for (int i=0;i<7;++i) f.tap(PET_X,PET_Y);
    const unsigned saves=f.store.saves;
    for (int i=0;i<500;++i) f.tick(20);
    CHECK(f.store.saves==saves);            // Nothing written while on screen.
    f.tap(240,NAV_Y); CHECK(f.manager.activeId()==AppId::Home && f.store.saves==saves+1);
    f.open(); f.tap(240,NAV_Y); CHECK(f.store.saves==saves+1);  // Unchanged: no write.
    store=f.store;
  }
  {
    TestClock clock; ProgressService progress; ProgressTestAccess::setFocusXP(progress,120);
    PetService pet(store,progress,clock); pet.begin(); pet.onVisit();
    CHECK(pet.loadResult()==BlobLoad::Loaded && pet.petCount()==7);
    CHECK(pet.event()==PetEvent::None && pet.mood()==PetMood::Idle);  // XP already seen.
  }
  const size_t offsets[4]={0,4,16,20};  // magic, version, style, checksum
  for (size_t off:offsets) {
    FakeBlobStore bad=store; bad.corrupt("deskos-pet","state",off);
    TestClock clock; ProgressService progress; ProgressTestAccess::setFocusXP(progress,120);
    PetService pet(bad,progress,clock); pet.begin();
    CHECK(pet.loadResult()==BlobLoad::Invalid && pet.petCount()==0 && pet.style()==PetStyle::Classic);
    CHECK(progress.totalXP()==120);  // Cosmetic reset only.
  }
  FakeBlobStore failing; failing.failWrites=true;
  TestClock clock; ProgressService progress; PetService pet(failing,progress,clock); pet.begin();
  pet.pet(); CHECK(!pet.persist() && pet.petCount()==1);  // RAM state kept; retried next exit.
  failing.failWrites=false; CHECK(pet.persist() && failing.writes==1);
}
void renderBoundsAndAllocations() {
  Fixture f; f.setXP(4600); f.open();
  auto& draw=f.display.graphics();
  CHECK(draw.clears==1 && draw.outside==0);
  const unsigned clears=draw.clears;
  { const unsigned long a=g_allocations; int* probe=new int(1); delete probe; CHECK(g_allocations==a+1); }
  const unsigned long allocations=g_allocations;
  for (int i=0;i<3000;++i) { f.tick(17); if (i%250==0) f.tap(PET_X,PET_Y); if (i%700==0) f.tap(STYLE_X,STYLE_Y); }
  f.tap(SLEEP_X,SLEEP_Y); for (int i=0;i<200;++i) f.tick(50);
  CHECK(draw.clears==clears && draw.outside==0);
  CHECK(g_allocations==allocations);  // No heap use in steady-state update/render/touch.
  // Between animation frames nothing is redrawn.
  f.tap(SLEEP_X,SLEEP_Y); f.tick(PetService::HAPPY_MS+10);
  f.clock.time+=600-f.clock.time%600+100; f.manager.update(0);  // Mid-frame for 600/200 ms phases.
  const unsigned before=draw.draws; f.tick(1); f.tick(1);
  CHECK(draw.draws==before);
  f.tap(80,NAV_Y); CHECK(f.manager.activeId()==AppId::Home);
}
}  // namespace
int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"P01 level thresholds, progress bar, unlock table, sprites",levelMath},
    {"P02 moods, TAP, sleep/wake, live level-up; XP never changed",moodsAndXpNeverChanges},
    {"P03 locked/unlocked styles and frame selection",unlocksAndStyles},
    {"P04 save on exit only, restore, corrupt record, write failure",persistenceAndCorruption},
    {"P05 320x240 bounds, no full redraw, zero steady-state allocations",renderBoundsAndAllocations}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u pet tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
