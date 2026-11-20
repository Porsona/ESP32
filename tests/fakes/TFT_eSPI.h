#pragma once
#include <stdint.h>
#include <cstring>
constexpr uint8_t TL_DATUM=0,TC_DATUM=1,TR_DATUM=2,MC_DATUM=4;
// Host-only draw spy: checks bounds/redraws, does not emulate display hardware.
class TFT_eSPI {
 public:
  unsigned draws=0,clears=0,outside=0;
  void fillScreen(uint16_t) { ++draws; ++clears; }
  void fillRect(int x,int y,int w,int h,uint16_t) { bounds(x,y,w,h); }
  void drawRect(int x,int y,int w,int h,uint16_t) { bounds(x,y,w,h); }
  void drawFastHLine(int x,int y,int w,uint16_t) { bounds(x,y,w,1); }
  void drawFastVLine(int x,int y,int h,uint16_t) { bounds(x,y,1,h); }
  void drawPixel(int x,int y,uint16_t) { bounds(x,y,1,1); }
  void setTextFont(uint8_t) {}
  void setTextSize(uint8_t size) { size_=size; }
  void setTextDatum(uint8_t datum) { datum_=datum; }
  void setTextColor(uint16_t,uint16_t) {}
  void drawString(const char* text,int x,int y) {
    const int w=std::strlen(text)*6*size_,h=8*size_;
    if (datum_==TC_DATUM || datum_==MC_DATUM) x-=w/2;
    else if (datum_==TR_DATUM) x-=w;
    if (datum_==MC_DATUM) y-=h/2;
    bounds(x,y,w,h);
  }
  // Optional "must not be painted" rectangles (e.g. window mullions).
  unsigned forbiddenHits=0;
  void forbid(int x,int y,int w,int h) { if (forbidCount_<8) { int* f=forbidden_[forbidCount_++]; f[0]=x; f[1]=y; f[2]=w; f[3]=h; } }
  void clearForbidden() { forbidCount_=0; }
 private:
  void bounds(int x,int y,int w,int h) {
    ++draws;
    if (x<0 || y<0 || w<0 || h<0 || x+w>320 || y+h>240) ++outside;
    for (int i=0;i<forbidCount_;++i) {
      const int* f=forbidden_[i];
      if (x<f[0]+f[2] && f[0]<x+w && y<f[1]+f[3] && f[1]<y+h) ++forbiddenHits;
    }
  }
  int forbidden_[8][4]={{0}};
  int forbidCount_=0;
  uint8_t size_=1,datum_=TL_DATUM;
};
