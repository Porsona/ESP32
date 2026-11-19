#include "HomeScreen.h"

namespace deskos {
void HomeScreen::onEnter() {
  pressed_=tapTarget_=-1; fullRedraw_=dirty_=true; dirtyRegions_=0xFF;
}
void HomeScreen::onExit() { pressed_=tapTarget_=-1; dirty_=false; }
void HomeScreen::invalidate(int region) {
  if (region>=0) { dirtyRegions_|=1U<<region; dirty_=true; }
}
int HomeScreen::hit(int16_t x,int16_t y) const {
  for (int i=0;i<6;++i) if (layout::Tiles[i].contains(x,y)) return i;
  // BACK is disabled on Home. Region 7 is the HOME navigation button.
  return layout::Home.contains(x,y)?7:-1;
}
void HomeScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press:
      pressed_=tapTarget_=target; invalidate(pressed_); break;
    case TouchEvent::Type::Move:
      if (tapTarget_!=target) { invalidate(pressed_); pressed_=tapTarget_=-1; }
      break;
    case TouchEvent::Type::Release:
      invalidate(pressed_); pressed_=-1;
      if (tapTarget_!=target) tapTarget_=-1;
      break;
    case TouchEvent::Type::Tap:
      if (tapTarget_>=0 && tapTarget_==target) {
        if (tapTarget_<6) manager_.open(static_cast<AppId>(tapTarget_+1));
        else manager_.home();
      }
      tapTarget_=-1; break;
    case TouchEvent::Type::Cancel:
      invalidate(pressed_); pressed_=tapTarget_=-1; break;
  }
}
void HomeScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("SHELL"); }
  for (int i=0;i<6;++i) {
    if (!(dirtyRegions_&(1U<<i))) continue;
    const auto& rect=layout::Tiles[i];
    const AppId id=static_cast<AppId>(i+1);
    const bool pressed=pressed_==i;
    renderer_.drawPanel(rect,pressed);
    renderer_.drawPixelIcon(id,rect.x+32,rect.y+8,2,pressed?theme::Background:theme::Mint);
    renderer_.drawText(appInfo(id).label,rect.x+rect.w/2,rect.y+53,1,
                       pressed?theme::Background:theme::Gold,
                       pressed?theme::Mint:theme::Panel,TC_DATUM);
  }
  if (dirtyRegions_&(1U<<6)) renderer_.drawButton(layout::Back,"BACK",false,false);
  if (dirtyRegions_&(1U<<7)) renderer_.drawButton(layout::Home,"HOME",pressed_==7);
  fullRedraw_=dirty_=false; dirtyRegions_=0;
}
}  // namespace deskos
