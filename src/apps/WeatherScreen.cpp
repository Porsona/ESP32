#include "WeatherScreen.h"
#include <cstdio>

namespace deskos {
namespace {
constexpr Rect HEADER={0,32,320,24};
constexpr Rect ICON={8,60,104,104};
constexpr int16_t ICON_X=10,ICON_Y=64,DRIFT=6;
constexpr uint8_t ICON_SCALE=6;
constexpr Rect INFO={120,58,200,98};
constexpr Rect FOOTER={0,166,192,32};
constexpr Rect REFRESH={196,158,116,40};
constexpr uint32_t FRAME_MS=600,INFO_TICK_MS=10000,NOTICE_MS=3000;
constexpr bool apart(const Rect& a,const Rect& b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
static_assert(apart(ICON,INFO) && apart(INFO,REFRESH) && apart(FOOTER,REFRESH) && apart(ICON,FOOTER) &&
              REFRESH.y+REFRESH.h<=200 && REFRESH.h>=40 && ICON_X+DRIFT+16*ICON_SCALE<=ICON.x+ICON.w,
              "Weather layout overlaps");
const SpritePalette PALETTES[3]={
  {theme::Background,theme::rgb565(0x8A,0x6A,0x20),theme::White,theme::rgb565(0x9A,0xA4,0xB8),
   theme::rgb565(0x7C,0xC4,0xFF),theme::White,theme::Gold},
  {theme::Background,theme::rgb565(0x40,0x48,0x58),theme::White,theme::rgb565(0x9A,0xA4,0xB8),
   theme::rgb565(0x7C,0xC4,0xFF),theme::White,theme::Gold},
  {theme::Background,theme::rgb565(0x20,0x24,0x2E),theme::rgb565(0x6A,0x72,0x82),theme::rgb565(0x3A,0x40,0x4E),
   theme::rgb565(0x7C,0xC4,0xFF),theme::White,theme::Gold}
};
}  // namespace

void WeatherScreen::onEnter() {
  pressed_=tapTarget_=NoTarget; notice_=nullptr; frame_=0;
  fullRedraw_=dirty_=true; regions_=All;
  lastRevision_=weather_.revision(); lastNet_=network_.state(); lastInfoTick_=clock_.nowMs();
  lastMinuteKey_=time_.valid()?time_.epoch()/60:0;
}
void WeatherScreen::onExit() { pressed_=tapTarget_=NoTarget; dirty_=false; }

void WeatherScreen::update(uint32_t deltaMs) {
  (void)deltaMs;
  const uint32_t now=clock_.nowMs();
  if (weather_.revision()!=lastRevision_ || network_.state()!=lastNet_) {
    lastRevision_=weather_.revision(); lastNet_=network_.state(); invalidate(Icon|Info|Footer);
  }
  const uint32_t minuteKey=time_.valid()?time_.epoch()/60:0;
  if (minuteKey!=lastMinuteKey_) { lastMinuteKey_=minuteKey; invalidate(Header); }
  if (now-lastInfoTick_>=INFO_TICK_MS) { lastInfoTick_=now; invalidate(Info|Footer); }  // "N MIN AGO".
  if (notice_ && now-noticeAt_>=NOTICE_MS) { notice_=nullptr; invalidate(Footer); }
  const uint8_t frame=static_cast<uint8_t>((now/FRAME_MS)%2);
  if (frame!=frame_ && weather_.hasReading()) { frame_=frame; invalidate(Icon); }
}

int WeatherScreen::hit(int16_t x,int16_t y) const {
  if (REFRESH.contains(x,y)) return Refresh;
  if (layout::Back.contains(x,y)) return Back;
  return layout::Home.contains(x,y)?Home:NoTarget;
}
void WeatherScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press: pressed_=tapTarget_=target; invalidate(Button|Nav); break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { pressed_=tapTarget_=NoTarget; invalidate(Button|Nav); }
      break;
    case TouchEvent::Type::Release:
      pressed_=NoTarget; invalidate(Button|Nav);
      if (target!=tapTarget_) tapTarget_=NoTarget;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=NoTarget;
      if (captured<0 || captured!=target) break;
      if (captured==Back) manager_.back();
      else if (captured==Home) manager_.home();
      else {
        if (!network_.connected()) notice_="WIFI NOT CONNECTED";
        else notice_=weather_.requestRefresh()?"REFRESH QUEUED":"PLEASE WAIT 30 S";
        noticeAt_=clock_.nowMs(); invalidate(Footer);
      }
      break;
    }
    case TouchEvent::Type::Cancel: pressed_=tapTarget_=NoTarget; invalidate(Button|Nav); break;
  }
}

void WeatherScreen::drawHeader() {
  renderer_.fillRegion(HEADER);
  renderer_.drawText(weather_.location().label,8,40,1,theme::Mint);
  char text[16];
  if (time_.valid()) {
    const uint32_t e=time_.epoch();
    std::snprintf(text,sizeof(text),"%02u:%02u",static_cast<unsigned>(localHour(e)),static_cast<unsigned>(localMinute(e)));
    renderer_.drawText(text,312,36,2,theme::Gold,theme::Background,TR_DATUM);
    renderer_.drawText("ICT",246,40,1,theme::Mint,theme::Background,TR_DATUM);
  } else renderer_.drawText("--:-- NO CLOCK",312,40,1,theme::Rose,theme::Background,TR_DATUM);
}
void WeatherScreen::drawIcon() {
  renderer_.fillRegion(ICON);
  if (!weather_.hasReading()) {
    renderer_.drawText("NO",ICON.x+ICON.w/2,ICON.y+38,2,theme::Rose,theme::Background,TC_DATUM);
    renderer_.drawText("DATA",ICON.x+ICON.w/2,ICON.y+58,2,theme::Rose,theme::Background,TC_DATUM);
    return;
  }
  const char* rows[WEATHER_SPRITE_SIZE];
  const WeatherReading& r=weather_.reading();
  const WeatherIcon icon=weatherIcon(weatherKind(r.code),r.isDay!=0,frame_,rows);
  const int16_t x=static_cast<int16_t>(ICON_X+(icon.drift && frame_?DRIFT:0));
  renderer_.drawCharSprite(rows,WEATHER_SPRITE_SIZE,WEATHER_SPRITE_SIZE,x,ICON_Y,ICON_SCALE,
                           PALETTES[static_cast<uint8_t>(icon.palette)]);
}
void WeatherScreen::drawInfo() {
  renderer_.fillRegion(INFO);
  char text[40];
  const int16_t x=INFO.x+4;
  if (weather_.hasReading()) {
    const WeatherReading& r=weather_.reading();
    const int t=r.temperatureDeci,whole=(t<0?-t:t)/10,tenth=(t<0?-t:t)%10;
    std::snprintf(text,sizeof(text),"%s%d.%d",t<0?"-":"",whole,tenth);
    renderer_.drawText(text,x,62,4,theme::Gold);
    const int16_t after=static_cast<int16_t>(x+static_cast<int16_t>(std::snprintf(nullptr,0,"%s",text))*24+4);
    renderer_.fillRegion({after,64,4,4},theme::Gold);            // Pixel degree sign.
    renderer_.fillRegion({static_cast<int16_t>(after+1),65,2,2},theme::Background);
    renderer_.drawText("C",after+8,64,2,theme::Gold);
    renderer_.drawText(weatherText(r.code),x,98,2,theme::Mint);
    std::snprintf(text,sizeof(text),"HUM %u%%  WIND %u.%u KM/H",static_cast<unsigned>(r.humidity),
                  static_cast<unsigned>(r.windDeci/10),static_cast<unsigned>(r.windDeci%10));
    renderer_.drawText(text,x,120,1,theme::White);
  } else renderer_.drawText("WAITING FOR DATA",x,80,2,theme::Rose);
  uint32_t age=0;
  if (weather_.fetching()) std::snprintf(text,sizeof(text),"UPDATING...");
  else if (!weather_.hasReading()) std::snprintf(text,sizeof(text),"NO WEATHER YET");
  else if (!weather_.ageSeconds(age)) std::snprintf(text,sizeof(text),"CACHED, AGE UNKNOWN");
  else if (age<60) std::snprintf(text,sizeof(text),"%s JUST NOW",weather_.source()==WeatherSource::Live?"UPDATED":"CACHED");
  else if (age<5400) std::snprintf(text,sizeof(text),"%s %lu MIN AGO",weather_.source()==WeatherSource::Live?"UPDATED":"CACHED",
                                   static_cast<unsigned long>(age/60));
  else std::snprintf(text,sizeof(text),"%s %lu H AGO%s",weather_.source()==WeatherSource::Live?"UPDATED":"CACHED",
                     static_cast<unsigned long>(age/3600),weather_.stale()?" (OLD)":"");
  renderer_.drawText(text,x,134,1,weather_.stale()?theme::Rose:theme::Mint);
  char ip[16];
  switch (network_.state()) {
    case NetState::Connected:
      formatIPv4(network_.ipv4(),ip,sizeof(ip));
      std::snprintf(text,sizeof(text),"WIFI %s %dDBM",ip,static_cast<int>(network_.rssi())); break;
    case NetState::Connecting: std::snprintf(text,sizeof(text),"WIFI CONNECTING - OFFLINE"); break;
    case NetState::NoConfig: std::snprintf(text,sizeof(text),"WIFI NOT SET (SERIAL: WIFI)"); break;
    default: std::snprintf(text,sizeof(text),"WIFI OFF - OFFLINE MODE"); break;
  }
  renderer_.drawText(text,x,146,1,network_.connected()?theme::Mint:theme::Rose);
}
void WeatherScreen::drawFooter() {
  renderer_.fillRegion(FOOTER);
  char text[40];
  if (notice_) std::snprintf(text,sizeof(text),"%s",notice_);
  else if (weather_.lastError()!=WeatherError::None)
    std::snprintf(text,sizeof(text),"ERR %s, RETRY %lus",weatherErrorName(weather_.lastError()),
                  static_cast<unsigned long>((weather_.msUntilNextAttempt()+999)/1000));
  else if (network_.connected() && !weather_.fetching())
    std::snprintf(text,sizeof(text),"NEXT UPDATE IN %lu MIN",static_cast<unsigned long>((weather_.msUntilNextAttempt()+59999)/60000));
  else std::snprintf(text,sizeof(text),"OPEN-METEO.COM");
  renderer_.drawText(text,8,172,1,notice_?theme::Gold:weather_.lastError()!=WeatherError::None?theme::Rose:theme::Mint);
  renderer_.drawText("DATA: OPEN-METEO (CC BY 4.0)",8,186,1,theme::Panel);
}

void WeatherScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("LIVING WEATHER"); regions_=All; }
  if (regions_&Header) drawHeader();
  if (regions_&Icon) drawIcon();
  if (regions_&Info) drawInfo();
  if (regions_&Footer) drawFooter();
  if (regions_&Button) renderer_.drawButton(REFRESH,"REFRESH",pressed_==Refresh,network_.connected());
  if (regions_&Nav) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==Back);
    renderer_.drawButton(layout::Home,"HOME",pressed_==Home);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
