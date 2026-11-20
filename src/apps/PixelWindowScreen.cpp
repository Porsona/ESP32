#include "PixelWindowScreen.h"
#include <cstring>

namespace deskos {
namespace {
constexpr Rect SCENE_BUTTON={256,36,56,78},SPEED_BUTTON={256,118,56,78};
constexpr uint8_t MAX_STEPS_PER_UPDATE=3;  // Bounded work after a long loop stall.
constexpr bool apart(const Rect& a,const Rect& b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
static_assert(apart(window::FRAME,SCENE_BUTTON) && apart(window::FRAME,SPEED_BUTTON) && apart(SCENE_BUTTON,SPEED_BUTTON) &&
              window::FRAME.y>=32 && window::FRAME.y+window::FRAME.h<=200 && SPEED_BUTTON.y+SPEED_BUTTON.h<=200,
              "Window layout overlaps");
}  // namespace

void PixelWindowScreen::begin() {
  loadResult_=loadWindowSettings(store_,settings_);
  saved_=settings_;
  if (loadResult_!=BlobLoad::Loaded) std::memset(&saved_,0,sizeof(saved_));  // Persist defaults on first exit.
}
WindowScene PixelWindowScreen::resolve() const {
  const WindowScene s=selected();
  if (s!=WindowScene::Auto) return s;
  if (!context_.clockValid()) return WindowScene::Day;  // Shown as "NO CLOCK": no guessed time.
  if (context_.raining()) return WindowScene::Rain;
  const uint8_t h=context_.localHour();
  return h>=6 && h<18?WindowScene::Day:WindowScene::Night;
}
void PixelWindowScreen::onEnter() {
  pressed_=tapTarget_=NoTarget; accumulator_=0; animating_=false;
  fullRedraw_=dirty_=true; regions_=All;
}
void PixelWindowScreen::onExit() {
  pressed_=tapTarget_=NoTarget; dirty_=false; animating_=false;
  sealWindowSettings(settings_);
  if (std::memcmp(&settings_,&saved_,sizeof(settings_)) && saveWindowSettings(store_,settings_)) saved_=settings_;
}
void PixelWindowScreen::update(uint32_t deltaMs) {
  if (resolve()!=animator_.scene() && !fullRedraw_) invalidate(Glass|Side);  // AUTO followed the clock/weather.
  const uint16_t period=speedPeriodMs(speed());
  accumulator_+=deltaMs;
  if (accumulator_>=period) {
    if (accumulator_>static_cast<uint32_t>(period)*MAX_STEPS_PER_UPDATE) accumulator_=static_cast<uint32_t>(period)*MAX_STEPS_PER_UPDATE;
    animating_=true; dirty_=true;  // Steps run in render(), never inside touch handling.
  }
}
int PixelWindowScreen::hit(int16_t x,int16_t y) const {
  if (window::FRAME.contains(x,y)) return GlassTap;
  if (SCENE_BUTTON.contains(x,y)) return SceneButton;
  if (SPEED_BUTTON.contains(x,y)) return SpeedButton;
  if (layout::Back.contains(x,y)) return Back;
  return layout::Home.contains(x,y)?Home:NoTarget;
}
void PixelWindowScreen::activate(int target) {
  switch (target) {
    case GlassTap:
    case SceneButton:
      settings_.scene=static_cast<uint8_t>((settings_.scene+1)%static_cast<uint8_t>(WindowScene::Count));
      invalidate(Glass|Side); break;   // Saved when leaving the app.
    case SpeedButton:
      settings_.speed=static_cast<uint8_t>((settings_.speed+1)%static_cast<uint8_t>(WindowSpeed::Count));
      accumulator_=0; invalidate(Side); break;
    case Back: manager_.back(); break;
    case Home: manager_.home(); break;
    default: break;
  }
}
void PixelWindowScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press:
      pressed_=tapTarget_=target;
      if (target==SceneButton || target==SpeedButton) invalidate(Side); else if (target>=Back) invalidate(Nav);
      break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { pressed_=tapTarget_=NoTarget; invalidate(Side|Nav); }
      break;
    case TouchEvent::Type::Release:
      pressed_=NoTarget; invalidate(Side|Nav);
      if (target!=tapTarget_) tapTarget_=NoTarget;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=NoTarget;
      if (captured>=0 && captured==target) activate(captured);
      break;
    }
    case TouchEvent::Type::Cancel: pressed_=tapTarget_=NoTarget; invalidate(Side|Nav); break;
  }
}
void PixelWindowScreen::drawSide() {
  const Rect buttons[2]={SCENE_BUTTON,SPEED_BUTTON};
  for (uint8_t i=0;i<2;++i) {
    const Rect& r=buttons[i];
    const bool pressed=pressed_==SceneButton+i;
    const uint16_t fill=pressed?theme::Mint:theme::Panel,ink=pressed?theme::Background:theme::Gold;
    renderer_.drawPanel(r,pressed);
    const int16_t cx=static_cast<int16_t>(r.x+r.w/2);
    renderer_.drawText(i?"SPEED":"SCENE",cx,r.y+14,1,pressed?theme::Background:theme::Rose,fill,TC_DATUM);
    if (i) renderer_.drawText(speedName(speed()),cx,r.y+36,1,ink,fill,TC_DATUM);
    else {
      renderer_.drawText(sceneName(selected()),cx,r.y+32,1,ink,fill,TC_DATUM);
      if (selected()==WindowScene::Auto)
        renderer_.drawText(context_.clockValid()?sceneName(resolve()):"NO CLOCK",cx,r.y+48,1,ink,fill,TC_DATUM);
    }
    renderer_.drawText("TAP",cx,r.y+r.h-16,1,pressed?theme::Background:theme::Mint,fill,TC_DATUM);
  }
}
void PixelWindowScreen::render() {
  if (fullRedraw_) {
    renderer_.clear(); renderer_.drawStatusBar("PIXEL WINDOW");
    animator_.drawFrame(); regions_=All;
  }
  if (regions_&Glass) { animator_.start(resolve()); accumulator_=0; animating_=false; }
  if (animating_) {
    const uint16_t period=speedPeriodMs(speed());
    while (accumulator_>=period) { animator_.step(); accumulator_-=period; }
    animating_=false;
  }
  if (regions_&Side) drawSide();
  if (regions_&Nav) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==Back);
    renderer_.drawButton(layout::Home,"HOME",pressed_==Home);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
