#include "PlaceholderScreen.h"

namespace deskos {
void PlaceholderScreen::onEnter() {
  id_=manager_.activeId(); pressed_=tapTarget_=-1;
  fullRedraw_=dirty_=true; dirtyRegions_=3;
}
void PlaceholderScreen::onExit() { pressed_=tapTarget_=-1; dirty_=false; }
void PlaceholderScreen::invalidate(int region) {
  if (region>=0) { dirtyRegions_|=1U<<region; dirty_=true; }
}
int PlaceholderScreen::hit(int16_t x,int16_t y) const {
  if (layout::Back.contains(x,y)) return 0;
  return layout::Home.contains(x,y)?1:-1;
}
void PlaceholderScreen::onTouch(const TouchEvent& event) {
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
        if (tapTarget_==0) manager_.back(); else manager_.home();
      }
      tapTarget_=-1; break;
    case TouchEvent::Type::Cancel:
      invalidate(pressed_); pressed_=tapTarget_=-1; break;
  }
}
void PlaceholderScreen::render() {
  if (fullRedraw_) {
    renderer_.clear(); renderer_.drawStatusBar(appInfo(id_).label);
    renderer_.drawPixelIcon(id_,136,56,3,theme::Mint);
    renderer_.drawText(appInfo(id_).name,160,116,2,theme::Rose,theme::Background,TC_DATUM);
    renderer_.drawText("COMING SOON",160,156,2,theme::Gold,theme::Background,TC_DATUM);
    renderer_.drawText("SHELL PLACEHOLDER",160,184,1,theme::Mint,theme::Background,TC_DATUM);
  }
  if (dirtyRegions_&1) renderer_.drawButton(layout::Back,"BACK",pressed_==0);
  if (dirtyRegions_&2) renderer_.drawButton(layout::Home,"HOME",pressed_==1);
  fullRedraw_=dirty_=false; dirtyRegions_=0;
}
}  // namespace deskos
