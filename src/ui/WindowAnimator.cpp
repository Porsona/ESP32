#include "WindowAnimator.h"

namespace deskos {
namespace {
using theme::rgb565;
constexpr int16_t BAND_TOP=40,BAND_H=20,GLASS_BOTTOM=192,RAIN_FLOOR=156,DROP_LEN=6;
constexpr uint8_t BANDS=6;
const uint16_t SKY[3][BANDS]={
  {rgb565(0x3A,0x8D,0xE0),rgb565(0x4F,0x9E,0xE8),rgb565(0x66,0xAE,0xEE),
   rgb565(0x7F,0xBF,0xF2),rgb565(0x99,0xCF,0xF5),rgb565(0xB5,0xDD,0xF7)},
  {rgb565(0x0B,0x0E,0x2A),rgb565(0x10,0x14,0x36),rgb565(0x16,0x1B,0x42),
   rgb565(0x1D,0x23,0x4F),rgb565(0x25,0x2C,0x5C),rgb565(0x2E,0x36,0x6A)},
  {rgb565(0x3E,0x44,0x52),rgb565(0x47,0x4E,0x5C),rgb565(0x51,0x58,0x66),
   rgb565(0x5B,0x63,0x70),rgb565(0x66,0x6E,0x7A),rgb565(0x71,0x79,0x84)}
};
const uint16_t HILL[3]={rgb565(0x4C,0xA8,0x5A),rgb565(0x1A,0x3A,0x3A),rgb565(0x3D,0x55,0x45)};
const uint16_t HILL_EDGE[3]={rgb565(0x72,0xE6,0xBA),rgb565(0x2E,0x5A,0x55),rgb565(0x5A,0x75,0x60)};
const uint16_t WOOD=rgb565(0x6B,0x4A,0x2F),WOOD_LIGHT=rgb565(0x9A,0x6E,0x45);
const uint16_t SUN=theme::Gold,SUN_GLOW[2]={rgb565(0xF8,0xE8,0xA8),rgb565(0xFF,0xF4,0xD0)};
const uint16_t MOON=rgb565(0xEE,0xEE,0xD8),MOON_GLOW[2]={rgb565(0x3A,0x44,0x7A),rgb565(0x4A,0x55,0x8C)};
const uint16_t STAR[3]={rgb565(0x55,0x5E,0x90),rgb565(0xA8,0xB0,0xD8),theme::White};
const uint16_t CLOUD=theme::White,RAIN_CLOUD=rgb565(0x2C,0x30,0x3A),FLASH=rgb565(0xE8,0xE8,0xF8);
const uint16_t DROP=rgb565(0x9C,0xC8,0xF0);
// Cloud rows as (startCell,lengthCells); one cell = 2 px. 40x12 px, convex per row.
const uint8_t CLOUD_RUNS[6][2]={{6,6},{3,12},{1,17},{0,20},{1,18},{4,10}};
constexpr int16_t CLOUD_W=40,CELL=2;
const int16_t SUN_X=52,SUN_Y=66,MOON_X=200,MOON_Y=64;

int16_t isqrt(int32_t v) {
  int32_t r=0;
  while ((r+1)*(r+1)<=v) ++r;
  return static_cast<int16_t>(r);
}
int16_t tri(int16_t v,int16_t period,int16_t amp) {
  const int16_t m=static_cast<int16_t>(v%period);
  const int16_t d=static_cast<int16_t>(m*2-period);
  return static_cast<int16_t>(amp*(d<0?-d:d)/period);
}
int16_t hillTop(int16_t x) { return static_cast<int16_t>(GLASS_BOTTOM-(12+tri(x,96,12)+tri(static_cast<int16_t>(x+30),40,4))); }
uint8_t sceneIndex(WindowScene s) { return s==WindowScene::Night?1:s==WindowScene::Rain?2:0; }
}  // namespace

const char* sceneName(WindowScene s) {
  switch (s) {
    case WindowScene::Night: return "NIGHT";
    case WindowScene::Rain: return "RAIN";
    case WindowScene::Auto: return "AUTO";
    default: return "DAY";
  }
}
const char* speedName(WindowSpeed s) { return s==WindowSpeed::Slow?"SLOW":s==WindowSpeed::Fast?"FAST":"NORMAL"; }
uint16_t speedPeriodMs(WindowSpeed s) { return s==WindowSpeed::Slow?140:s==WindowSpeed::Fast?35:70; }

uint32_t WindowAnimator::random() {  // xorshift32: deterministic, no allocation.
  seed_^=seed_<<13; seed_^=seed_>>17; seed_^=seed_<<5;
  return seed_;
}
uint16_t WindowAnimator::sky(int16_t y) const {
  int16_t band=static_cast<int16_t>((y-BAND_TOP)/BAND_H);
  if (band<0) band=0;
  if (band>=BANDS) band=BANDS-1;
  return SKY[sceneIndex(scene_)][band];
}
void WindowAnimator::paneFill(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color) {
  if (w<=0 || h<=0) return;
  for (const Rect& p:window::PANES) {
    const int16_t x0=x>p.x?x:p.x,y0=y>p.y?y:p.y;
    const int16_t x1=x+w<p.x+p.w?x+w:p.x+p.w,y1=y+h<p.y+p.h?y+h:p.y+p.h;
    if (x1>x0 && y1>y0) {
      renderer_.fillRegion({x0,y0,static_cast<int16_t>(x1-x0),static_cast<int16_t>(y1-y0)},color);
      ++fills_;
    }
  }
}
void WindowAnimator::restore(int16_t x,int16_t y,int16_t w,int16_t h) {
  int16_t row=y;
  while (row<y+h) {  // Split at sky band boundaries.
    int16_t band=static_cast<int16_t>((row-BAND_TOP)/BAND_H);
    if (row<BAND_TOP) band=0;
    const int16_t bandEnd=band>=BANDS-1?GLASS_BOTTOM:static_cast<int16_t>(BAND_TOP+(band+1)*BAND_H);
    const int16_t end=bandEnd<y+h?bandEnd:static_cast<int16_t>(y+h);
    if (end<=row) break;
    paneFill(x,row,w,static_cast<int16_t>(end-row),sky(row));
    row=end;
  }
}
void WindowAnimator::drawFrame() {
  renderer_.fillRegion(window::FRAME,WOOD);
  renderer_.fillRegion({window::FRAME.x,window::FRAME.y,window::FRAME.w,1},WOOD_LIGHT);
  renderer_.fillRegion({window::FRAME.x,window::FRAME.y,1,window::FRAME.h},WOOD_LIGHT);
  renderer_.fillRegion(window::MULLION_V,WOOD);
  renderer_.fillRegion(window::MULLION_H,WOOD);
  renderer_.fillRegion({window::MULLION_V.x,window::MULLION_V.y,1,window::MULLION_V.h},WOOD_LIGHT);
  renderer_.fillRegion({window::MULLION_H.x,window::MULLION_H.y,window::MULLION_H.w,1},WOOD_LIGHT);
}
void WindowAnimator::drawHills() {
  const uint8_t s=sceneIndex(scene_);
  for (int16_t x=window::GLASS.x;x<window::GLASS.x+window::GLASS.w;x+=4) {
    const int16_t top=hillTop(x);
    paneFill(x,top,4,static_cast<int16_t>(GLASS_BOTTOM-top),HILL[s]);
    paneFill(x,top,4,2,HILL_EDGE[s]);
  }
}
void WindowAnimator::drawDisc(int16_t cx,int16_t cy,int16_t r,uint16_t color,bool skyColor) {
  for (int16_t dy=static_cast<int16_t>(-r);dy<=r;++dy) {
    const int16_t hw=isqrt(r*r-dy*dy);
    if (skyColor) restore(static_cast<int16_t>(cx-hw),static_cast<int16_t>(cy+dy),static_cast<int16_t>(2*hw+1),1);
    else paneFill(static_cast<int16_t>(cx-hw),static_cast<int16_t>(cy+dy),static_cast<int16_t>(2*hw+1),1,color);
  }
}
void WindowAnimator::drawRing(int16_t cx,int16_t cy,int16_t inner,int16_t outer,uint16_t color,bool skyColor) {
  for (int16_t dy=static_cast<int16_t>(-outer);dy<=outer;++dy) {
    const int16_t ho=isqrt(outer*outer-dy*dy);
    const int16_t ady=dy<0?static_cast<int16_t>(-dy):dy;
    const int16_t hi=ady<=inner?isqrt(inner*inner-dy*dy):static_cast<int16_t>(-1);
    const int16_t y=static_cast<int16_t>(cy+dy);
    const int16_t lx=static_cast<int16_t>(cx-ho),lw=static_cast<int16_t>(hi<0?2*ho+1:ho-hi);
    const int16_t rx=static_cast<int16_t>(cx+hi+1),rw=static_cast<int16_t>(hi<0?0:ho-hi);
    if (skyColor) { restore(lx,y,lw,1); restore(rx,y,rw,1); }
    else { paneFill(lx,y,lw,1,color); paneFill(rx,y,rw,1,color); }
  }
}
void WindowAnimator::drawCloud(const Cloud& c,bool full) {
  for (uint8_t row=0;row<6;++row) {
    const int16_t y=static_cast<int16_t>(c.y+row*CELL);
    const int16_t start=static_cast<int16_t>(c.x+CLOUD_RUNS[row][0]*CELL);
    const int16_t end=static_cast<int16_t>(start+CLOUD_RUNS[row][1]*CELL);
    if (full) paneFill(start,y,static_cast<int16_t>(end-start),CELL,CLOUD);
    else {  // Moved one cell right: erase trailing cell, paint leading cell.
      restore(static_cast<int16_t>(start-CELL),y,CELL,CELL);
      paneFill(static_cast<int16_t>(end-CELL),y,CELL,CELL,CLOUD);
    }
  }
}
void WindowAnimator::drawRainClouds(bool flash) {
  for (int16_t x=window::GLASS.x,i=0;x<window::GLASS.x+window::GLASS.w;x+=8,++i) {
    const int16_t h=static_cast<int16_t>(20+(i*5)%9);
    paneFill(x,window::GLASS.y,8,h,flash?FLASH:RAIN_CLOUD);
  }
}
void WindowAnimator::resetDrop(Drop& d,bool anywhere) {
  d.x=static_cast<int16_t>(window::GLASS.x+random()%window::GLASS.w);
  d.y=static_cast<int16_t>(anywhere?70+random()%(RAIN_FLOOR-DROP_LEN-70):70+random()%8);
  d.speed=static_cast<uint8_t>(3+random()%3);
}

void WindowAnimator::start(WindowScene resolved) {
  scene_=resolved==WindowScene::Auto?WindowScene::Day:resolved;
  steps_=0; glowPhase_=0; flash_=0; fills_=0;
  restore(window::GLASS.x,window::GLASS.y,window::GLASS.w,window::GLASS.h);
  drawHills();
  if (scene_==WindowScene::Day) {
    drawDisc(SUN_X,SUN_Y,12,SUN);
    const int16_t lanes[CLOUDS]={86,100,124};
    const uint8_t every[CLOUDS]={3,4,5};
    for (uint8_t i=0;i<CLOUDS;++i) {
      clouds_[i].x=static_cast<int16_t>(20+i*76); clouds_[i].y=lanes[i]; clouds_[i].every=every[i];
      drawCloud(clouds_[i],true);
    }
  } else if (scene_==WindowScene::Night) {
    drawDisc(MOON_X,MOON_Y,11,MOON);
    drawDisc(MOON_X+5,MOON_Y-4,9,0,true);  // Crescent: carve with sky.
    for (auto& s:stars_) {
      do {
        s.x=static_cast<int16_t>(14+random()%228); s.y=static_cast<int16_t>(42+random()%66);
      } while (s.x>178 && s.x<222 && s.y<86);  // Keep clear of the moon and its glow.
      s.level=static_cast<uint8_t>(random()%3);
      paneFill(s.x,s.y,2,2,STAR[s.level]);
    }
  } else {
    drawRainClouds(false);
    for (auto& d:drops_) { resetDrop(d,true); paneFill(d.x,d.y,1,DROP_LEN,DROP); }
  }
  lastFills_=fills_;
}

void WindowAnimator::step() {
  ++steps_; fills_=0;
  if (scene_==WindowScene::Day) {
    for (auto& c:clouds_) {
      if (steps_%c.every) continue;
      c.x=static_cast<int16_t>(c.x+CELL);
      // Wrap once fully past the glass; re-enters from the left edge by edge-painting.
      if (c.x>=window::GLASS.x+window::GLASS.w) c.x=static_cast<int16_t>(window::GLASS.x-CLOUD_W-CELL*(random()%8));
      drawCloud(c,false);
    }
    if (steps_%6==0) {
      glowPhase_=static_cast<uint8_t>((glowPhase_+1)%4);  // off, soft, bright, soft
      drawRing(SUN_X,SUN_Y,14,17,glowPhase_==0?0:SUN_GLOW[glowPhase_==2],glowPhase_==0);
    }
  } else if (scene_==WindowScene::Night) {
    for (uint8_t i=0;i<2;++i) {
      Star& s=stars_[random()%STARS];
      s.level=static_cast<uint8_t>(random()%3);
      paneFill(s.x,s.y,2,2,STAR[s.level]);
    }
    if (steps_%10==0) {
      glowPhase_=static_cast<uint8_t>((glowPhase_+1)%4);
      drawRing(MOON_X,MOON_Y,13,16,glowPhase_==0?0:MOON_GLOW[glowPhase_==2],glowPhase_==0);
    }
  } else {
    for (auto& d:drops_) {
      const int16_t next=static_cast<int16_t>(d.y+d.speed);
      if (next+DROP_LEN>RAIN_FLOOR) {
        restore(d.x,d.y,1,DROP_LEN); resetDrop(d,false); paneFill(d.x,d.y,1,DROP_LEN,DROP);
      } else {  // Erase the vacated top, paint the new bottom: no flicker.
        restore(d.x,d.y,1,d.speed); paneFill(d.x,static_cast<int16_t>(d.y+DROP_LEN),1,d.speed,DROP);
        d.y=next;
      }
    }
    if (flash_) { if (--flash_==0) drawRainClouds(false); }  // Lightning: brief light, then restore.
    else if (random()%220==0) { flash_=3; drawRainClouds(true); }
  }
  lastFills_=fills_;
}
}  // namespace deskos
