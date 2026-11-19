#pragma once
#include "DisplayHAL.h"
#include "UITheme.h"

namespace deskos {
class PixelRenderer {
 public:
  explicit PixelRenderer(DisplayHAL& display) : display_(display) {}
  void clear();
  void drawPixelIcon(AppId id,int16_t x,int16_t y,uint8_t scale,uint16_t ink);
  void drawPanel(const Rect& rect,bool pressed=false,bool enabled=true);
  void drawButton(const Rect& rect,const char* text,bool pressed=false,bool enabled=true);
  void drawText(const char* text,int16_t x,int16_t y,uint8_t size=1,
                uint16_t ink=theme::White,uint16_t background=theme::Background,uint8_t datum=TL_DATUM);
  void drawStatusBar(const char* tag);
 private:
  DisplayHAL& display_;
};
}  // namespace deskos
