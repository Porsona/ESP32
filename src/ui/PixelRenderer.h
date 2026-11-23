#pragma once
#include "DisplayHAL.h"
#include "UITheme.h"
#include "SpriteInk.h"

namespace deskos {
// Character sprites: '.' background, '#' outline, 'o' body, '+' shade,
// '*' accent, 'w' white, 'g' gold. Unknown characters draw as background.
struct SpritePalette { uint16_t background,outline,body,shade,accent,white,gold; };

class PixelRenderer {
 public:
  explicit PixelRenderer(DisplayHAL& display) : display_(display) {}
  void clear();
  void fillRegion(const Rect& rect,uint16_t color=theme::Background);
  void drawProgressBar(const Rect& rect,uint16_t permille);
  void drawPixelIcon(AppId id,int16_t x,int16_t y,uint8_t scale,uint16_t ink);
  void drawPanel(const Rect& rect,bool pressed=false,bool enabled=true);
  void drawButton(const Rect& rect,const char* text,bool pressed=false,bool enabled=true,uint8_t textSize=2);
  // 8x8 one-bit glyph, each set bit drawn as a scale x scale square (flash constant).
  void drawGlyph8(const uint8_t rows[8],int16_t x,int16_t y,uint8_t scale,uint16_t ink);
  void drawCheckbox(const Rect& rect,bool checked);
  // Every cell is painted (background included), so redrawing a frame over the
  // previous one never flickers and needs no framebuffer.
  void drawCharSprite(const char* const* rows,uint8_t width,uint8_t height,int16_t x,int16_t y,
                      uint8_t scale,const SpritePalette& palette);
  // Multi-tone variant: characters looked up in an ink table; unknown ones draw as background.
  void drawCharSprite(const char* const* rows,uint8_t width,uint8_t height,int16_t x,int16_t y,
                      uint8_t scale,const SpriteInk* inks,uint8_t inkCount,uint16_t background);
  void drawText(const char* text,int16_t x,int16_t y,uint8_t size=1,
                uint16_t ink=theme::White,uint16_t background=theme::Background,uint8_t datum=TL_DATUM);
  void drawStatusBar(const char* tag);
 private:
  DisplayHAL& display_;
};
}  // namespace deskos
