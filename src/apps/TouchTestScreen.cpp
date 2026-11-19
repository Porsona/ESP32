#include "TouchTestScreen.h"
#include <cstdio>

namespace {
struct Zone { int16_t x,y; };
constexpr Zone ZONES[]={{10,56},{230,56},{120,116},{10,176},{230,176}};
constexpr int ZONE_W=80, ZONE_H=50;
static_assert(ZONE_W>=40 && ZONE_H>=40,"Touch test hitboxes must be >=40px");
}

void TouchTestScreen::begin() {
  cursorVisible_=false; waitForRelease_=true; tapCount_=0; lastHit_=-1; lastDraw_=0;
  display_.graphics().fillScreen(TFT_BLACK); drawStatic();
  Serial.println("TOUCH_TEST: START zones=5 size=80x50");
}

int TouchTestScreen::hit(const TouchPixel& p) const {
  for (int i=0;i<5;++i) {
    const auto& z=ZONES[i];
    if (p.x>=z.x && p.x<z.x+ZONE_W && p.y>=z.y && p.y<z.y+ZONE_H) return i;
  }
  return -1;
}

void TouchTestScreen::drawStatic() {
  auto& tft=display_.graphics();
  tft.setTextFont(1); tft.setTextDatum(TL_DATUM); tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("CYD DESKOS - TOUCH TEST",8,6);
  for (int i=0;i<5;++i) {
    const auto& z=ZONES[i];
    tft.drawRect(z.x,z.y,ZONE_W,ZONE_H,i==lastHit_?TFT_GREEN:TFT_WHITE);
    char label[12]; snprintf(label,sizeof(label),"ZONE %d",i+1);
    tft.drawString(label,z.x+20,z.y+21);
  }
  tft.drawString("Serial: CAL recalibrate | DIAG recovery",8,230);
}

void TouchTestScreen::update(const TouchFrame& frame, const touchcal::CalibrationRecord& calibration,
                             uint32_t now) {
  if (!frame.updated) return;
  if (waitForRelease_) {
    if (!frame.down && !frame.rawValid) waitForRelease_=false;
    return;
  }
  const TouchPixel pixel=touchcal::map(calibration,frame.raw);
  if (frame.events & TOUCH_TAP) {
    ++tapCount_; lastHit_=hit(pixel);
    Serial.printf("TOUCH_TEST: HIT zone=%d taps=%lu\n",lastHit_+1,static_cast<unsigned long>(tapCount_));
  }
  if (!frame.events && (!frame.down || !frame.rawValid || now-lastDraw_<100)) return;
  lastDraw_=now;
  auto& tft=display_.graphics();
  if (cursorVisible_) tft.fillRect(cursor_.x-9,cursor_.y-9,19,19,TFT_BLACK);
  // Restore outlines/text under the old cursor, without a screen-size buffer.
  drawStatic();
  tft.fillRect(0,22,320,26,TFT_BLACK);
  char line[72];
  const char* event=(frame.events&TOUCH_TAP)?"TAP":(frame.events&TOUCH_RELEASE)?"RELEASE":
                    frame.heldMs>700?"HOLD":"PRESS";
  snprintf(line,sizeof(line),"X=%d Y=%d Z=%d %s taps=%lu",pixel.x,pixel.y,frame.raw.z,event,
           static_cast<unsigned long>(tapCount_));
  tft.drawString(line,8,24);
  snprintf(line,sizeof(line),"RAW %d,%d | hit=%d (0=outside)",frame.raw.x,frame.raw.y,lastHit_+1);
  tft.drawString(line,8,38);
  cursor_=pixel; cursorVisible_=true;
  tft.drawFastHLine(pixel.x-8,pixel.y,17,TFT_YELLOW);
  tft.drawFastVLine(pixel.x,pixel.y-8,17,TFT_YELLOW);
}
