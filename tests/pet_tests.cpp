#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include "TestSupport.h"
#include "apps/PixelPetScreen.h"
#include "ui/PetSpriteData.h"

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
  // Styles keep the robot; crown is an overlay, LED eyes express the mood.
  CHECK(petFrame(PetMood::Idle,PetStyle::Crown,0,0).crown);
  CHECK(petFrame(PetMood::Sleep,PetStyle::Classic,0,0).eyes==PetEyes::Sleepy);
  CHECK(petFrame(PetMood::Idle,PetStyle::Classic,3100,0).eyes==PetEyes::Closed);
  CHECK(petFrame(PetMood::Happy,PetStyle::HeartEyes,0,0).eyes==PetEyes::Heart);
}
bool isLed(char ch) { return ch=='e' || ch=='g' || ch=='o' || ch=='w'; }  // LED colours and their glint.
bool sitting(PetPose pose) { return pose==PetPose::Sleep || pose==PetPose::SleepBreathe; }
bool hanging(PetPose pose) { return pose==PetPose::Hang || pose==PetPose::HangSwing; }
bool symmetricPose(PetPose pose) {
  return pose==PetPose::Relaxed || pose==PetPose::Breathe || sitting(pose) || pose==PetPose::Cheer ||
         pose==PetPose::CheerBounce || pose==PetPose::Laugh;
}
unsigned countIn(const PetCanvas& c,uint8_t x0,uint8_t y0,uint8_t w,uint8_t h,const char* keys) {
  unsigned n=0;
  for (uint8_t y=y0;y<y0+h;++y) for (uint8_t x=x0;x<x0+w;++x) n+=std::strchr(keys,c.rows[y][x])!=nullptr;
  return n;
}
void voltFrames() {
  CHECK(PET_SPRITE_SIZE==48 && PET_SPRITE_SIZE*2==96 && petSpritesWellFormed() && petInkCount()==11);
  // LED cells on the visor per expression: open, blink, happy, sleep, star (+gold), heart (+rose).
  const unsigned LED[6]={52,20,24,14,4,4},STAR_GOLD=32,HEART_ROSE=36,SKIP_ROWS=13;
  bool seen[256]={false};
  for (uint8_t pose=0;pose<static_cast<uint8_t>(PetPose::Count);++pose) {
    for (uint8_t eyes=0;eyes<static_cast<uint8_t>(PetEyes::Count);++eyes) for (uint8_t crown=0;crown<2;++crown) {
      const PetFrame frame={static_cast<PetPose>(pose),static_cast<PetEyes>(eyes),crown!=0};
      const uint8_t key=petFrameKey(frame);
      CHECK(!seen[key]); seen[key]=true;  // Every visual component affects invalidation.
      PetCanvas c; std::memset(&c,'?',sizeof(c)); composePet(c,frame);
      for (uint8_t y=0;y<PET_SPRITE_SIZE;++y) {
        CHECK(std::strlen(c.rows[y])==PET_SPRITE_SIZE);
        for (uint8_t x=0;x<PET_SPRITE_SIZE;++x) CHECK(std::strchr(".#hmMvepPgow",c.rows[y][x]));
      }
      const unsigned all=PET_SPRITE_SIZE;
      CHECK(countIn(c,0,0,all,all,"hmM")>=190 && countIn(c,0,0,all,all,"pP")>=90);  // Mint helmet, purple body.
      CHECK(countIn(c,0,0,all,all,"#")>=250 && countIn(c,0,0,all,all,"v")>=130);    // Outline, dark visor.
      CHECK(countIn(c,0,0,all,all,"g")>=40 && countIn(c,0,0,all,all,"o")>=20);      // Gold bolts/boots, rose mitts.
      if (hanging(frame.pose)) continue;  // Checked against the Cheer frame below.
      const int8_t dx=petart::HEAD_OFFSET[pose][0],dy=petart::HEAD_OFFSET[pose][1];
      const uint8_t ex=static_cast<uint8_t>(petart::EYE_X+dx),ey=static_cast<uint8_t>(petart::EYE_Y+dy);
      CHECK(countIn(c,ex,ey,petart::EYE_W,petart::EYE_H,"e")==LED[eyes]);
      CHECK(countIn(c,ex,ey,petart::EYE_W,petart::EYE_H,"g")==(eyes==static_cast<uint8_t>(PetEyes::Star)?STAR_GOLD:0U));
      CHECK(countIn(c,ex,ey,petart::EYE_W,petart::EYE_H,"o")==(eyes==static_cast<uint8_t>(PetEyes::Heart)?HEART_ROSE:0U));
      const uint8_t top=petHeadTop(frame.pose);
      CHECK(top==petart::HEAD_TOP+dy);
      const unsigned crownGold=countIn(c,static_cast<uint8_t>(petart::CROWN_X+dx),static_cast<uint8_t>(petart::CROWN_Y+dy),
                                       petart::CROWN_W,petart::CROWN_H,"g");
      CHECK(crownGold==(crown?34U:0U));  // Crown follows the head in every upright pose.
      if (!crown) for (uint8_t y=0;y<top;++y) CHECK(std::strspn(c.rows[y],".")==PET_SPRITE_SIZE);
      if (symmetricPose(frame.pose) && dx==0) {  // Mirrored helmet and LED eyes below the antenna and visor glint.
        for (uint8_t y=top+SKIP_ROWS;y<=petart::HEAD_BOTTOM+dy;++y) for (uint8_t x=0;x<PET_SPRITE_SIZE/2;++x) {
          const char a=c.rows[y][x],b=c.rows[y][PET_SPRITE_SIZE-1-x];
          CHECK((a=='.')==(b=='.') && isLed(a)==isLed(b));
        }
      }
      unsigned segments=0; bool inside=false;
      for (uint8_t x=0;x<PET_SPRITE_SIZE;++x) { const bool on=c.rows[46][x]!='.'; segments+=on && !inside; inside=on; }
      CHECK(segments==(sitting(frame.pose)?1U:2U));  // Two boots standing; crossed legs when sitting.
    }
  }
  // Arms: one raised to wave, both raised to cheer, none at rest; the cable leaves the hand.
  const PetPose raised[4]={PetPose::Wave,PetPose::WaveFlick,PetPose::Cheer,PetPose::Relaxed};
  for (PetPose pose:raised) {
    PetCanvas c; composePet(c,{pose,PetEyes::Open,false});
    const bool left=countIn(c,0,14,8,13,"pPo")>0,right=countIn(c,40,14,8,13,"pPo")>0;
    CHECK(right==(pose!=PetPose::Relaxed) && left==(pose==PetPose::Cheer));
  }
  PetCanvas cable; composePet(cable,{PetPose::GrappleLeft,PetEyes::Open,false});
  CHECK(cable.rows[petCableRow()][0]=='w' && countIn(cable,0,petCableRow(),PET_SPRITE_SIZE,1,"w")>=1);
  composePet(cable,{PetPose::GrappleRight,PetEyes::Open,false});
  CHECK(cable.rows[petCableRow()][PET_SPRITE_SIZE-1]=='w');
  // Hang = Cheer flipped upside down (+ cable); the swing frame is shifted sideways.
  for (uint8_t crown=0;crown<2;++crown) {
    PetCanvas cheer,hang,swing;
    composePet(cheer,{PetPose::Cheer,PetEyes::Happy,crown!=0});
    composePet(hang,{PetPose::Hang,PetEyes::Happy,crown!=0});
    composePet(swing,{PetPose::HangSwing,PetEyes::Happy,crown!=0});
    unsigned strand=0;
    for (uint8_t y=0;y<PET_SPRITE_SIZE;++y) for (uint8_t x=0;x<PET_SPRITE_SIZE;++x) {
      const char flipped=cheer.rows[PET_SPRITE_SIZE-1-y][x],h=hang.rows[y][x];
      if (h=='w' && flipped=='.') { ++strand; CHECK(x==PET_SPRITE_SIZE/2-1); }
      else CHECK(h==flipped);
      const char shifted=x>=PET_HANG_SWING?cheer.rows[PET_SPRITE_SIZE-1-y][x-PET_HANG_SWING]:'.';
      if (swing.rows[y][x]=='w' && shifted=='.') CHECK(x==PET_SPRITE_SIZE/2-1+PET_HANG_SWING);
      else CHECK(swing.rows[y][x]==shifted);
    }
    CHECK(strand>=2 && hang.rows[0][PET_SPRITE_SIZE/2-1]=='w');
  }
  const uint32_t times[]={0,249,250,599,600,899,900,3049,3050,3199,3200,UINT32_MAX};
  for (uint8_t style=0;style<PET_STYLE_COUNT;++style) for (uint8_t mood=0;mood<4;++mood) {
    for (uint32_t time:times) {
      const PetFrame f=petFrame(static_cast<PetMood>(mood),static_cast<PetStyle>(style),time,time);
      CHECK(static_cast<uint8_t>(f.pose)<static_cast<uint8_t>(PetPose::Count));
      CHECK(f.crown==(style==static_cast<uint8_t>(PetStyle::Crown) || style==static_cast<uint8_t>(PetStyle::Aura)));
      if (mood==static_cast<uint8_t>(PetMood::Sleep)) CHECK(f.eyes==PetEyes::Sleepy);
    }
  }
  CHECK(petFrameKey(petFrame(PetMood::Happy,PetStyle::Classic,0,0))!=
        petFrameKey(petFrame(PetMood::Happy,PetStyle::Classic,0,250)));
  CHECK(petFrameKey(petFrame(PetMood::Celebrate,PetStyle::Classic,0,0))!=
        petFrameKey(petFrame(PetMood::Celebrate,PetStyle::Classic,0,250)));
  CHECK(petFrameKey(petFrame(PetMood::Sleep,PetStyle::Classic,0,0))!=
        petFrameKey(petFrame(PetMood::Sleep,PetStyle::Classic,0,900)));
  PetCanvas fallback,normal;
  composePet(fallback,{static_cast<PetPose>(255),static_cast<PetEyes>(255),false});
  composePet(normal,{PetPose::Relaxed,PetEyes::Open,false});
  CHECK(!std::memcmp(&fallback,&normal,sizeof(normal)) && petHeadTop(static_cast<PetPose>(255))==petart::HEAD_TOP);
}
void voltLabelsAndBounds() {
  Fixture f;
  auto& draw=f.display.graphics();
  draw.watchText("SPIDER","VOLT");
  f.open();
  f.tap(PET_X,PET_Y); f.tick(PetService::HAPPY_MS);
  f.tap(SLEEP_X,SLEEP_Y); f.tick(950); f.tap(PET_X,PET_Y);
  f.setXP(25); f.tick(10);  // XP gain text.
  f.setXP(100); f.tick(10); // Level-up text.
  f.setXP(UINT32_MAX); f.tick(10); // Longest XP header; unlock notification.
  for (int i=0;i<5;++i) { f.tap(STYLE_X,STYLE_Y); f.tick(250); }
  f.tap(SLEEP_X,SLEEP_Y); f.tap(SLEEP_X,SLEEP_Y);
  CHECK(draw.textMatches[0]==0 && draw.textMatches[1]>=5);
  CHECK(draw.outside==0 && f.progress.totalXP()==UINT32_MAX);
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
void touchEvent(Fixture& f,TouchEvent::Type type,int16_t x,int16_t y,uint32_t heldMs) {
  f.manager.onTouch({type,x,y,f.clock.time,heldMs});
}
void interactions() {
  Fixture f; f.setXP(50); f.open(); f.tick(PetService::HAPPY_MS);  // Let the XP greeting finish.
  auto& draw=f.display.graphics();
  const unsigned saves=f.store.saves; const uint32_t xp=f.progress.totalXP();
  const unsigned long allocations=g_allocations;
  const int16_t HEAD_Y=80,BELLY_Y=150,LEFT_X=96,RIGHT_X=224;
  CHECK(HEAD_Y<68+(petHeadBottom()+1)*2 && BELLY_Y>=68+(petHeadBottom()+1)*2);
  // Head tap = pat: wave + hearts, PETS +1.
  f.tap(PET_X,HEAD_Y);
  CHECK(f.pet.mood()==PetMood::Happy && f.pet.event()==PetEvent::Petted && f.pet.petCount()==1);
  CHECK(f.pet.reaction()==PetReaction::None);
  // Belly tap = tickle: laughing frames, PETS +1, then back to idle.
  f.tap(PET_X,BELLY_Y);
  CHECK(f.pet.reaction()==PetReaction::Tickle && f.pet.event()==PetEvent::Tickled && f.pet.petCount()==2);
  CHECK(f.pet.mood()==PetMood::Idle);
  const PetFrame laugh=petFrame(f.pet.mood(),f.pet.reaction(),f.pet.style(),0,0,0);
  CHECK(laugh.pose==PetPose::Laugh && laugh.eyes==PetEyes::Happy);
  CHECK(petFrame(PetMood::Idle,PetReaction::Tickle,PetStyle::Classic,0,150,0).pose==PetPose::LaughShake);
  f.tick(PetService::REACTION_MS); CHECK(f.pet.reaction()==PetReaction::None);
  // Empty sides = grapple shot towards that side; no PETS.
  f.tap(LEFT_X,110);
  CHECK(f.pet.reaction()==PetReaction::GrappleLeft && f.pet.event()==PetEvent::Grapple && f.pet.eventValue()==0);
  f.tap(RIGHT_X,110);
  CHECK(f.pet.reaction()==PetReaction::GrappleRight && f.pet.eventValue()==1 && f.pet.petCount()==2);
  CHECK(petFrame(PetMood::Idle,PetReaction::GrappleRight,PetStyle::Classic,0,0,0).pose==PetPose::GrappleRight);
  f.tick(PetService::REACTION_MS);
  // Stroke (finger travels >8 px): AppManager cancels the tap, the pet gets one pat; no tap after it.
  touchEvent(f,TouchEvent::Type::Press,130,100,0);
  touchEvent(f,TouchEvent::Type::Move,150,104,80);
  CHECK(f.pet.petCount()==3 && f.pet.mood()==PetMood::Happy && f.pet.event()==PetEvent::Petted);
  touchEvent(f,TouchEvent::Type::Release,170,104,120); touchEvent(f,TouchEvent::Type::Tap,170,104,120);
  f.manager.update(0); CHECK(f.pet.petCount()==3);
  // Long press (>700 ms, finger still) = hang upside down for HANG_MS.
  touchEvent(f,TouchEvent::Type::Press,PET_X,120,0);
  touchEvent(f,TouchEvent::Type::Move,PET_X+2,121,750);
  CHECK(f.pet.reaction()==PetReaction::Hang && f.pet.event()==PetEvent::Hanging && f.pet.petCount()==3);
  touchEvent(f,TouchEvent::Type::Release,PET_X+2,121,900); f.manager.update(0);
  CHECK(petFrame(f.pet.mood(),f.pet.reaction(),f.pet.style(),0,0,0).pose==PetPose::Hang);
  CHECK(petFrame(f.pet.mood(),f.pet.reaction(),f.pet.style(),0,400,0).pose==PetPose::HangSwing);
  f.tick(1000); CHECK(f.pet.reaction()==PetReaction::Hang);
  f.tick(PetService::HANG_MS); CHECK(f.pet.reaction()==PetReaction::None);
  // A long press on a button is not a pet gesture.
  touchEvent(f,TouchEvent::Type::Press,STYLE_X,STYLE_Y,0);
  touchEvent(f,TouchEvent::Type::Move,STYLE_X,STYLE_Y,750);
  CHECK(f.pet.reaction()==PetReaction::None);
  touchEvent(f,TouchEvent::Type::Release,STYLE_X,STYLE_Y,800); f.manager.update(0);
  // A sleeping pet wakes up on a tickle.
  f.tap(SLEEP_X,SLEEP_Y); CHECK(f.pet.mood()==PetMood::Sleep);
  f.tap(PET_X,BELLY_Y);
  CHECK(f.pet.mood()==PetMood::Idle && f.pet.event()==PetEvent::Woke && f.pet.reaction()==PetReaction::Tickle);
  // Sleep/style buttons stop a reaction; reactions never touch XP or NVS while on screen.
  f.tap(SLEEP_X,SLEEP_Y); CHECK(f.pet.reaction()==PetReaction::None && f.pet.mood()==PetMood::Sleep);
  f.tap(SLEEP_X,SLEEP_Y);
  for (int i=0;i<300;++i) { f.tick(23); if (i%60==0) f.tap(i%120?PET_X:LEFT_X,i%120?BELLY_Y:110); }
  CHECK(f.progress.totalXP()==xp && f.store.saves==saves && g_allocations==allocations);
  CHECK(draw.outside==0 && draw.clears==1);
  // A press on the pet interrupted by an app switch (Cancel at 0,0) does nothing.
  f.tick(PetService::REACTION_MS); CHECK(f.pet.reaction()==PetReaction::None);
  touchEvent(f,TouchEvent::Type::Press,PET_X,BELLY_Y,0);
  f.manager.open(AppId::Home); f.manager.update(0);
  CHECK(f.manager.activeId()==AppId::Home && f.pet.reaction()==PetReaction::None);
  CHECK(f.store.saves==saves+1);  // Leaving the app saves the new PETS count once.
}
}  // namespace
int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"P01 level thresholds, progress bar, unlock table, sprites",levelMath},
    {"P02 moods, TAP, sleep/wake, live level-up; XP never changed",moodsAndXpNeverChanges},
    {"P03 locked/unlocked styles and frame selection",unlocksAndStyles},
    {"P04 save on exit only, restore, corrupt record, write failure",persistenceAndCorruption},
    {"P05 320x240 bounds, no full redraw, zero steady-state allocations",renderBoundsAndAllocations},
    {"P06 VOLT poses/LED eyes/styles, robot identity, unique frame keys",voltFrames},
    {"P07 VOLT labels, extreme XP bounds, no old pet names",voltLabelsAndBounds},
    {"P08 touch: head pat, belly tickle, side grapple, stroke, hold-to-hang, wake; no XP/NVS/heap",interactions}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u pet tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
