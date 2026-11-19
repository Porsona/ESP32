#include "PixelPetScreen.h"
#include <cstdio>

namespace deskos {
namespace {
constexpr Rect HEADER={0,32,320,26};
constexpr Rect XP_BAR={72,40,156,10};
constexpr Rect INFO={8,62,76,100};
constexpr Rect PET_AREA={88,58,144,116};   // Sprite 96x96 at (112,68) + effect margins.
constexpr int16_t SPRITE_X=112,SPRITE_Y=68;
constexpr uint8_t SPRITE_SCALE=6;
constexpr Rect STYLE_BUTTON={236,62,76,46},SLEEP_BUTTON={236,114,76,46};
constexpr Rect MESSAGE={0,178,320,18};
constexpr bool apart(const Rect& a,const Rect& b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
static_assert(apart(PET_AREA,STYLE_BUTTON) && apart(PET_AREA,SLEEP_BUTTON) && apart(STYLE_BUTTON,SLEEP_BUTTON) &&
              apart(INFO,PET_AREA) && PET_AREA.y+PET_AREA.h<=MESSAGE.y && MESSAGE.y+MESSAGE.h<=200,
              "Pet layout overlaps");
static_assert(SPRITE_X>=PET_AREA.x && SPRITE_X+16*SPRITE_SCALE<=PET_AREA.x+PET_AREA.w &&
              SPRITE_Y+16*SPRITE_SCALE<=PET_AREA.y+PET_AREA.h,"Sprite outside its area");
const SpritePalette PALETTE={theme::Background,theme::rgb565(0x1F,0x6E,0x5A),theme::Mint,
                             theme::rgb565(0x4F,0xB8,0x92),theme::Rose,theme::White,theme::Gold};
const uint8_t HEART[8]={0x00,0x66,0xFF,0xFF,0x7E,0x3C,0x18,0x00};
const uint8_t SPARK[8]={0x10,0x10,0x54,0x38,0xFE,0x38,0x54,0x10};
}  // namespace

void PixelPetScreen::onEnter() {
  pet_.onVisit();
  pressed_=tapTarget_=NoTarget; fullRedraw_=dirty_=true; regions_=All;
  lastRevision_=pet_.revision(); lastXP_=pet_.xp(); lastFrame_=frameKey(); lastExtras_=extrasKey();
}
void PixelPetScreen::onExit() {
  pressed_=tapTarget_=NoTarget; dirty_=false;
  pet_.persist();  // Lifecycle event, outside render; skipped when nothing changed.
}

uint8_t PixelPetScreen::frameKey() const {
  const uint32_t now=pet_.nowMs();
  const PetFrame f=petFrame(pet_.mood(),pet_.style(),now-pet_.moodSinceMs(),now);
  return static_cast<uint8_t>(static_cast<uint8_t>(f.head)*32+static_cast<uint8_t>(f.eyes)*4+static_cast<uint8_t>(f.mouth));
}
uint8_t PixelPetScreen::extrasKey() const {
  const uint32_t now=pet_.nowMs();
  const PetMood mood=pet_.mood();
  if (mood==PetMood::Sleep) return static_cast<uint8_t>(10+(now/900)%2);
  if (mood==PetMood::Happy) return static_cast<uint8_t>(20+(now/300)%2);
  if (mood==PetMood::Celebrate || pet_.style()==PetStyle::Aura) return static_cast<uint8_t>(30+(now/200)%3);
  return 0;
}

void PixelPetScreen::update(uint32_t deltaMs) {
  (void)deltaMs;
  pet_.update();
  if (pet_.revision()!=lastRevision_) { lastRevision_=pet_.revision(); invalidate(Info|Message|Side); }
  if (pet_.xp()!=lastXP_) { lastXP_=pet_.xp(); invalidate(Header); }
  // Animation advances only when the visible frame changes: a few redraws/second.
  const uint8_t frame=frameKey(),extras=extrasKey();
  if (frame!=lastFrame_) { lastFrame_=frame; invalidate(Sprite); }
  if (extras!=lastExtras_) { lastExtras_=extras; invalidate(Extras); }
}

int PixelPetScreen::hit(int16_t x,int16_t y) const {
  if (PET_AREA.contains(x,y)) return PetBody;
  if (STYLE_BUTTON.contains(x,y)) return Style;
  if (SLEEP_BUTTON.contains(x,y)) return Sleep;
  if (layout::Back.contains(x,y)) return Back;
  return layout::Home.contains(x,y)?Home:NoTarget;
}
void PixelPetScreen::invalidateTarget(int target) {
  if (target==Style || target==Sleep) invalidate(Side);
  else if (target==Back || target==Home) invalidate(Nav);
}
void PixelPetScreen::activate(int target) {
  switch (target) {
    case PetBody: pet_.pet(); break;
    case Style: pet_.cycleStyle(); invalidate(Sprite|Extras); break;
    case Sleep: pet_.toggleSleep(); break;
    case Back: manager_.back(); break;
    case Home: manager_.home(); break;
    default: break;
  }
}
void PixelPetScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press: pressed_=tapTarget_=target; invalidateTarget(target); break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget; }
      break;
    case TouchEvent::Type::Release:
      invalidateTarget(pressed_); pressed_=NoTarget;
      if (target!=tapTarget_) tapTarget_=NoTarget;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=NoTarget;
      if (captured>=0 && captured==target) activate(captured);
      break;
    }
    case TouchEvent::Type::Cancel: invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget; break;
  }
}

void PixelPetScreen::drawHeader() {
  renderer_.fillRegion(HEADER);
  char text[24];
  std::snprintf(text,sizeof(text),"LV %u",static_cast<unsigned>(pet_.level()));
  renderer_.drawText(text,8,37,2,theme::Gold);
  renderer_.drawProgressBar(XP_BAR,pet_.levelProgress());
  std::snprintf(text,sizeof(text),"XP %lu",static_cast<unsigned long>(pet_.xp()));
  renderer_.drawText(text,312,35,1,theme::Gold,theme::Background,TR_DATUM);
  if (pet_.level()<PET_MAX_LEVEL) std::snprintf(text,sizeof(text),"NEXT %lu",static_cast<unsigned long>(pet_.nextLevelXP()));
  else std::snprintf(text,sizeof(text),"MAX LEVEL");
  renderer_.drawText(text,312,46,1,theme::Mint,theme::Background,TR_DATUM);
}
void PixelPetScreen::drawInfo() {
  renderer_.fillRegion(INFO);
  char text[16];
  renderer_.drawText("MOOD",INFO.x,INFO.y+2,1,theme::Rose);
  renderer_.drawText(moodName(pet_.mood()),INFO.x,INFO.y+14,1,theme::Mint);
  renderer_.drawText("STYLE",INFO.x,INFO.y+34,1,theme::Rose);
  renderer_.drawText(styleName(pet_.style()),INFO.x,INFO.y+46,1,theme::Mint);
  renderer_.drawText("PETS",INFO.x,INFO.y+66,1,theme::Rose);
  std::snprintf(text,sizeof(text),"%lu",static_cast<unsigned long>(pet_.petCount()));
  renderer_.drawText(text,INFO.x,INFO.y+78,1,theme::Mint);
}
void PixelPetScreen::drawSprite() {
  const uint32_t now=pet_.nowMs();
  const PetFrame f=petFrame(pet_.mood(),pet_.style(),now-pet_.moodSinceMs(),now);
  const char* rows[PET_SPRITE_SIZE];
  composePet(rows,f.head,f.eyes,f.mouth);
  renderer_.drawCharSprite(rows,PET_SPRITE_SIZE,PET_SPRITE_SIZE,SPRITE_X,SPRITE_Y,SPRITE_SCALE,PALETTE);
}
void PixelPetScreen::drawExtras() {
  // Clear only the margins around the sprite, then draw the effect phase.
  renderer_.fillRegion({PET_AREA.x,PET_AREA.y,PET_AREA.w,SPRITE_Y-PET_AREA.y});
  renderer_.fillRegion({PET_AREA.x,SPRITE_Y+96,PET_AREA.w,PET_AREA.y+PET_AREA.h-(SPRITE_Y+96)});
  renderer_.fillRegion({PET_AREA.x,SPRITE_Y,SPRITE_X-PET_AREA.x,96});
  renderer_.fillRegion({SPRITE_X+96,SPRITE_Y,PET_AREA.x+PET_AREA.w-(SPRITE_X+96),96});
  const uint8_t key=lastExtras_;
  if (key==10 || key==11) {
    renderer_.drawText("z",212,96,1,theme::Mint);
    if (key==11) renderer_.drawText("Z",214,72,2,theme::Mint);
  } else if (key==20 || key==21) {
    renderer_.drawGlyph8(HEART,key==20?92:212,key==20?80:96,2,theme::Rose);
    renderer_.drawGlyph8(HEART,key==20?212:92,key==20?128:136,2,theme::Rose);
  } else if (key>=30) {
    // All positions lie in the margins, never on the sprite cells.
    const int16_t big[3][2]={{92,74},{212,120},{150,58}},small[3][2]={{94,140},{214,84},{170,166}};
    const uint8_t i=static_cast<uint8_t>((key-30)%3);
    renderer_.drawGlyph8(SPARK,big[i][0],big[i][1],i==2?1:2,theme::Gold);
    renderer_.drawGlyph8(SPARK,small[i][0],small[i][1],1,theme::Gold);
  }
}
void PixelPetScreen::drawMessage() {
  renderer_.fillRegion(MESSAGE);
  char text[40];
  const unsigned long v=pet_.eventValue();
  switch (pet_.event()) {
    case PetEvent::XpGained: std::snprintf(text,sizeof(text),"+%lu XP! BIT IS HAPPY",v); break;
    case PetEvent::LevelUp: std::snprintf(text,sizeof(text),"LEVEL UP! NOW LV %lu",v); break;
    case PetEvent::StyleUnlocked:
      std::snprintf(text,sizeof(text),"LV %u! NEW STYLE: %s",static_cast<unsigned>(pet_.level()),
                    styleName(static_cast<PetStyle>(v)));
      break;
    case PetEvent::Petted: std::snprintf(text,sizeof(text),"BIT LOVES PETS (%lu)",v); break;
    case PetEvent::Woke: std::snprintf(text,sizeof(text),"BIT WOKE UP"); break;
    case PetEvent::Slept: std::snprintf(text,sizeof(text),"ZZZ... TAP BIT TO WAKE"); break;
    case PetEvent::StyleChanged: std::snprintf(text,sizeof(text),"STYLE: %s",styleName(pet_.style())); break;
    case PetEvent::StyleLocked: std::snprintf(text,sizeof(text),"NEXT STYLE UNLOCKS AT LV %lu",v); break;
    default: std::snprintf(text,sizeof(text),"TAP BIT TO PLAY - XP FROM FOCUS/QUESTS"); break;
  }
  renderer_.drawText(text,160,MESSAGE.y+5,1,pet_.event()==PetEvent::None?theme::Mint:theme::Gold,
                     theme::Background,TC_DATUM);
}

void PixelPetScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("PIXEL PET"); regions_=All; }
  if (regions_&Header) drawHeader();
  if (regions_&Info) drawInfo();
  if (regions_&Sprite) drawSprite();
  if (regions_&Extras) drawExtras();
  if (regions_&Side) {
    renderer_.drawButton(STYLE_BUTTON,"STYLE",pressed_==Style,true,1);
    renderer_.drawButton(SLEEP_BUTTON,pet_.mood()==PetMood::Sleep?"WAKE":"SLEEP",pressed_==Sleep,true,1);
  }
  if (regions_&Message) drawMessage();
  if (regions_&Nav) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==Back);
    renderer_.drawButton(layout::Home,"HOME",pressed_==Home);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
