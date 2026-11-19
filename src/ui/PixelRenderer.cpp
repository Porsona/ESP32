#include "PixelRenderer.h"

namespace deskos {
namespace {
// Six hand-drawn 16x16 monochrome glyphs, flash constants, not framebuffers.
const uint16_t ICONS[6][16]={
  {0x300C,0x781E,0x7FFE,0x7FFE,0x6006,0x6246,0x6246,0x6006,
   0x6186,0x63C6,0x3FFC,0x1FF8,0x0FF0,0x0C30,0x1830,0x0000},
  {0xFFFF,0x8001,0xB801,0xB801,0x8001,0x8001,0x8181,0x83C1,
   0x87E1,0x8FF1,0x9FF9,0xBFFD,0xFFFF,0x8001,0x8001,0xFFFF},
  {0x0180,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,0x03C0,
   0x03C0,0x1FF8,0x0FF0,0x0180,0x0180,0x03C0,0x03C0,0x0000},
  {0x7FFE,0x4002,0x4C7A,0x4C02,0x407A,0x4002,0x4C7A,0x4C02,
   0x407A,0x4002,0x4C7A,0x4C02,0x4002,0x7FFE,0x6006,0x3FFC},
  {0xFFFF,0x8001,0xBFFD,0xA005,0xA005,0xA005,0xA005,0xA005,
   0xBFFD,0x8001,0xFFFF,0x0180,0x0180,0x07E0,0x0FF0,0x0000},
  {0x0180,0x2184,0x1008,0x07E0,0x0FF0,0xCFF3,0x0FF0,0x07E0,
   0x03F0,0x0FFC,0x1FFE,0x3FFF,0x3FFF,0x1FFE,0x0000,0x0000}
};
}

void PixelRenderer::clear() { display_.graphics().fillScreen(theme::Background); }
void PixelRenderer::fillRegion(const Rect& r,uint16_t color) {
  display_.graphics().fillRect(r.x,r.y,r.w,r.h,color);
}
void PixelRenderer::drawProgressBar(const Rect& r,uint16_t permille) {
  if (r.w<4 || r.h<4) return;
  auto& tft=display_.graphics();
  tft.drawRect(r.x,r.y,r.w,r.h,theme::Gold);
  tft.fillRect(r.x+1,r.y+1,r.w-2,r.h-2,theme::Background);
  const uint16_t bounded=permille>1000?1000:permille;
  const int16_t filled=static_cast<uint32_t>(r.w-4)*bounded/1000;
  if (filled) tft.fillRect(r.x+2,r.y+2,filled,r.h-4,theme::Mint);
}

void PixelRenderer::drawText(const char* text,int16_t x,int16_t y,uint8_t size,
                             uint16_t ink,uint16_t background,uint8_t datum) {
  auto& tft=display_.graphics();
  tft.setTextFont(1); tft.setTextSize(size); tft.setTextDatum(datum);
  tft.setTextColor(ink,background); tft.drawString(text,x,y);
  tft.setTextDatum(TL_DATUM);
}

void PixelRenderer::drawPixelIcon(AppId id,int16_t x,int16_t y,uint8_t scale,uint16_t ink) {
  const uint8_t index=static_cast<uint8_t>(id);
  if (index<1 || index>6 || scale==0) return;
  auto& tft=display_.graphics();
  for (int row=0;row<16;++row) {
    const uint16_t bits=ICONS[index-1][row];
    for (int col=0;col<16;) {
      if (!(bits&(0x8000U>>col))) { ++col; continue; }
      const int first=col;
      while (col<16 && (bits&(0x8000U>>col))) ++col;
      tft.fillRect(x+first*scale,y+row*scale,(col-first)*scale,scale,ink);
    }
  }
}

void PixelRenderer::drawPanel(const Rect& r,bool pressed,bool enabled) {
  auto& tft=display_.graphics();
  const uint16_t fill=!enabled?theme::Background:pressed?theme::Mint:theme::Panel;
  const uint16_t border=!enabled?theme::Panel:pressed?theme::Gold:theme::Mint;
  tft.fillRect(r.x+2,r.y,r.w-4,r.h,fill);
  tft.fillRect(r.x,r.y+2,r.w,r.h-4,fill);
  tft.drawFastHLine(r.x+2,r.y,r.w-4,border);
  tft.drawFastHLine(r.x+2,r.y+r.h-1,r.w-4,border);
  tft.drawFastVLine(r.x,r.y+2,r.h-4,border);
  tft.drawFastVLine(r.x+r.w-1,r.y+2,r.h-4,border);
  tft.drawPixel(r.x+1,r.y+1,border); tft.drawPixel(r.x+r.w-2,r.y+1,border);
  tft.drawPixel(r.x+1,r.y+r.h-2,border); tft.drawPixel(r.x+r.w-2,r.y+r.h-2,border);
}

void PixelRenderer::drawButton(const Rect& r,const char* text,bool pressed,bool enabled) {
  drawPanel(r,pressed,enabled);
  const uint16_t fill=!enabled?theme::Background:pressed?theme::Mint:theme::Panel;
  const uint16_t ink=!enabled?theme::Panel:pressed?theme::Background:theme::Gold;
  drawText(text,r.x+r.w/2,r.y+r.h/2,2,ink,fill,MC_DATUM);
}

void PixelRenderer::drawStatusBar(const char* tag) {
  auto& tft=display_.graphics();
  tft.fillRect(0,0,320,32,theme::Panel);
  tft.drawFastHLine(0,31,320,theme::Mint);
  drawText("DESKOS M1",8,8,2,theme::Gold,theme::Panel);
  drawText(tag,312,12,1,theme::Mint,theme::Panel,TR_DATUM);
}
}  // namespace deskos
