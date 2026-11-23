#include "PixelPetScreen.h"
#include <cstdio>

namespace deskos {
namespace {
constexpr Rect HEADER={0,32,320,26};
constexpr Rect XP_BAR={72,40,156,10};
constexpr Rect INFO={8,62,76,100};
constexpr Rect PET_AREA={88,58,144,116};   // Sprite 96x96 at (112,68) + effect margins.
constexpr int16_t SPRITE_X=112,SPRITE_Y=68;
constexpr uint8_t SPRITE_SCALE=2;
constexpr int16_t SPRITE_PIXELS=PET_SPRITE_SIZE*SPRITE_SCALE;
constexpr Rect STYLE_BUTTON={236,62,76,46},SLEEP_BUTTON={236,114,76,46};
constexpr Rect MESSAGE={0,178,320,18};
constexpr bool apart(const Rect& a,const Rect& b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
static_assert(apart(PET_AREA,STYLE_BUTTON) && apart(PET_AREA,SLEEP_BUTTON) && apart(STYLE_BUTTON,SLEEP_BUTTON) &&
              apart(INFO,PET_AREA) && PET_AREA.y+PET_AREA.h<=MESSAGE.y && MESSAGE.y+MESSAGE.h<=200,
              "Pet layout overlaps");
static_assert(SPRITE_X>=PET_AREA.x && SPRITE_X+SPRITE_PIXELS<=PET_AREA.x+PET_AREA.w &&
              SPRITE_Y+SPRITE_PIXELS<=PET_AREA.y+PET_AREA.h,"Sprite outside its area");
static_assert(SPRITE_PIXELS==96,"VOLT keeps the 96x96 pet area");
const uint8_t HEART[8]={0x00,0x66,0xFF,0xFF,0x7E,0x3C,0x18,0x00};
const uint8_t SPARK[8]={0x10,0x10,0x54,0x38,0xFE,0x38,0x54,0x10};
const uint8_t HOOK[8]={0x18,0x18,0x18,0x18,0x99,0x5A,0x3C,0x00};  // Grapple anchor.
constexpr int32_t STROKE_DIST2=64;  // Same threshold AppManager uses to cancel a tap.
constexpr uint32_t HOLD_MS=700;
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

PetFrame PixelPetScreen::currentFrame() const {
  const uint32_t now=pet_.nowMs();
  return petFrame(pet_.mood(),pet_.reaction(),pet_.style(),now-pet_.moodSinceMs(),now-pet_.reactionSinceMs(),now);
}
uint8_t PixelPetScreen::frameKey() const { return petFrameKey(currentFrame()); }
uint8_t PixelPetScreen::extrasKey() const {
  const uint32_t now=pet_.nowMs();
  const uint32_t since=now-pet_.reactionSinceMs();
  switch (pet_.reaction()) {
    case PetReaction::Tickle: return static_cast<uint8_t>(60+(since/300)%2);
    case PetReaction::GrappleLeft: return 40;
    case PetReaction::GrappleRight: return 41;
    case PetReaction::Hang: return static_cast<uint8_t>(50+(since/400)%2);  // Strand follows the swing.
    default: break;
  }
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
    case PetBody: interact(tapX_,tapY_); break;
    case Style: pet_.cycleStyle(); invalidate(Sprite|Extras); break;
    case Sleep: pet_.toggleSleep(); break;
    case Back: manager_.back(); break;
    case Home: manager_.home(); break;
    default: break;
  }
}
// Tap zones on the pet: head = pat, body = tickle, empty sides = grapple shot towards that side.
void PixelPetScreen::interact(int16_t x,int16_t y) {
  const bool onSprite=x>=SPRITE_X && x<SPRITE_X+SPRITE_PIXELS && y>=SPRITE_Y && y<SPRITE_Y+SPRITE_PIXELS;
  if (!onSprite) pet_.react(x<SPRITE_X+SPRITE_PIXELS/2?PetReaction::GrappleLeft:PetReaction::GrappleRight);
  else if (y<SPRITE_Y+(petHeadBottom()+1)*SPRITE_SCALE) pet_.pet();
  else pet_.react(PetReaction::Tickle);
}
void PixelPetScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press:
      pressed_=tapTarget_=target; pressX_=event.x; pressY_=event.y; invalidateTarget(target);
      break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget; }
      break;
    case TouchEvent::Type::Release:
      invalidateTarget(pressed_); pressed_=NoTarget;
      if (target!=tapTarget_) tapTarget_=NoTarget;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=NoTarget;
      tapX_=event.x; tapY_=event.y;
      if (captured>=0 && captured==target) activate(captured);
      break;
    }
    case TouchEvent::Type::Cancel: {
      // AppManager cancels a tap once the finger travels >8 px or stays >700 ms: on the pet that
      // gesture is a stroke (pat) or a long press (hang). App switches cancel at (0,0): ignored.
      const bool onPet=tapTarget_==PetBody && PET_AREA.contains(event.x,event.y);
      const int32_t dx=event.x-pressX_,dy=event.y-pressY_;
      invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget;
      if (onPet && dx*dx+dy*dy>STROKE_DIST2) pet_.pet();
      else if (onPet && event.heldMs>HOLD_MS) pet_.react(PetReaction::Hang);
      break;
    }
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
  composePet(canvas_,currentFrame());
  const char* rows[PET_SPRITE_SIZE];
  for (uint8_t y=0;y<PET_SPRITE_SIZE;++y) rows[y]=canvas_.rows[y];
  renderer_.drawCharSprite(rows,PET_SPRITE_SIZE,PET_SPRITE_SIZE,SPRITE_X,SPRITE_Y,SPRITE_SCALE,
                           petInks(),petInkCount(),theme::Background);
}
void PixelPetScreen::drawExtras() {
  // Clear only the margins around the sprite, then draw the effect phase.
  renderer_.fillRegion({PET_AREA.x,PET_AREA.y,PET_AREA.w,SPRITE_Y-PET_AREA.y});
  renderer_.fillRegion({PET_AREA.x,SPRITE_Y+SPRITE_PIXELS,PET_AREA.w,PET_AREA.y+PET_AREA.h-(SPRITE_Y+SPRITE_PIXELS)});
  renderer_.fillRegion({PET_AREA.x,SPRITE_Y,SPRITE_X-PET_AREA.x,SPRITE_PIXELS});
  renderer_.fillRegion({SPRITE_X+SPRITE_PIXELS,SPRITE_Y,PET_AREA.x+PET_AREA.w-(SPRITE_X+SPRITE_PIXELS),SPRITE_PIXELS});
  const uint8_t key=lastExtras_;
  if (key==40 || key==41) {  // Grapple cable continues from the hand to an anchor at the edge.
    const int16_t y=SPRITE_Y+petCableRow()*SPRITE_SCALE;
    if (key==40) {
      renderer_.fillRegion({PET_AREA.x+8,y,SPRITE_X-(PET_AREA.x+8),2},theme::White);
      renderer_.drawGlyph8(HOOK,PET_AREA.x,y-3,1,theme::Gold);
    } else {
      renderer_.fillRegion({SPRITE_X+SPRITE_PIXELS,y,PET_AREA.x+PET_AREA.w-8-(SPRITE_X+SPRITE_PIXELS),2},theme::White);
      renderer_.drawGlyph8(HOOK,PET_AREA.x+PET_AREA.w-8,y-3,1,theme::Gold);
    }
  } else if (key==50 || key==51) {  // Hanging: cable from the top of the pet area down to the sprite.
    const int16_t x=SPRITE_X+(PET_SPRITE_SIZE/2-1+(key==51?PET_HANG_SWING:0))*SPRITE_SCALE;
    renderer_.fillRegion({x,PET_AREA.y,SPRITE_SCALE,SPRITE_Y-PET_AREA.y},theme::White);
  } else if (key==60 || key==61) {  // Tickle laughter on alternating sides.
    renderer_.drawText("HA!",key==60?92:212,key==60?84:124,1,theme::Gold);
    renderer_.drawText("HA!",key==60?212:92,key==60?100:140,1,theme::Rose);
  } else if (key==10 || key==11) {
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
    case PetEvent::XpGained: std::snprintf(text,sizeof(text),"+%lu XP! VOLT IS HAPPY",v); break;
    case PetEvent::LevelUp: std::snprintf(text,sizeof(text),"LEVEL UP! NOW LV %lu",v); break;
    case PetEvent::StyleUnlocked:
      std::snprintf(text,sizeof(text),"LV %u! NEW STYLE: %s",static_cast<unsigned>(pet_.level()),
                    styleName(static_cast<PetStyle>(v)));
      break;
    case PetEvent::Petted: std::snprintf(text,sizeof(text),"VOLT LOVES PETS (%lu)",v); break;
    case PetEvent::Woke: std::snprintf(text,sizeof(text),"VOLT WOKE UP"); break;
    case PetEvent::Slept: std::snprintf(text,sizeof(text),"ZZZ... TAP VOLT TO WAKE"); break;
    case PetEvent::StyleChanged: std::snprintf(text,sizeof(text),"STYLE: %s",styleName(pet_.style())); break;
    case PetEvent::StyleLocked: std::snprintf(text,sizeof(text),"NEXT STYLE UNLOCKS AT LV %lu",v); break;
    case PetEvent::Tickled: std::snprintf(text,sizeof(text),"HAHA! THAT TICKLES (%lu)",v); break;
    case PetEvent::Grapple: std::snprintf(text,sizeof(text),"ZAP! GRAPPLE %s",v?"RIGHT":"LEFT"); break;
    case PetEvent::Hanging: std::snprintf(text,sizeof(text),"JUST HANGING AROUND..."); break;
    default: std::snprintf(text,sizeof(text),"TAP HEAD/BELLY/SIDE, SWIPE OR HOLD"); break;
  }
  renderer_.drawText(text,160,MESSAGE.y+5,1,pet_.event()==PetEvent::None?theme::Mint:theme::Gold,
                     theme::Background,TC_DATUM);
}

void PixelPetScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("VOLT"); regions_=All; }
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
