#include "PcMonitorScreen.h"
#include <cstdio>

namespace deskos {
namespace {
constexpr Rect HEADER={0,32,320,24};
constexpr Rect METERS={0,58,320,74};
constexpr Rect DETAILS={0,132,320,32};
constexpr Rect FOOTER={0,164,320,34};
constexpr int16_t BAR_X=48,BAR_W=180,BAR_H=14;
static_assert(FOOTER.y+FOOTER.h<=200 && HEADER.y>=32,"PC monitor layout outside content area");
uint16_t meterColor(uint16_t permille) {
  return permille>=900?theme::Rose:permille>=700?theme::Gold:theme::Mint;
}
}  // namespace

void PcMonitorScreen::onEnter() {
  pressed_=tapTarget_=-1; fullRedraw_=dirty_=true; regions_=All;
  lastRevision_=monitor_.revision(); lastState_=monitor_.state(network_.connected());
  lastSecond_=clock_.nowMs()/1000;
}
void PcMonitorScreen::onExit() { pressed_=tapTarget_=-1; dirty_=false; }

void PcMonitorScreen::update(uint32_t deltaMs) {
  (void)deltaMs;
  const PcLinkState s=monitor_.state(network_.connected());
  if (s!=lastState_) { lastState_=s; invalidate(Header|Meters|Details|Footer); }  // e.g. agent timed out.
  if (monitor_.revision()!=lastRevision_) { lastRevision_=monitor_.revision(); invalidate(Header|Meters|Details|Footer); }
  const uint32_t second=clock_.nowMs()/1000;
  if (second!=lastSecond_) { lastSecond_=second; invalidate(Footer); }  // "LAST n S AGO".
}

void PcMonitorScreen::onTouch(const TouchEvent& event) {
  const int target=layout::Back.contains(event.x,event.y)?0:layout::Home.contains(event.x,event.y)?1:-1;
  switch (event.type) {
    case TouchEvent::Type::Press: pressed_=tapTarget_=target; invalidate(Nav); break;
    case TouchEvent::Type::Move: if (target!=tapTarget_) { pressed_=tapTarget_=-1; invalidate(Nav); } break;
    case TouchEvent::Type::Release: pressed_=-1; invalidate(Nav); if (target!=tapTarget_) tapTarget_=-1; break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=-1;
      if (captured==0 && target==0) manager_.back();
      else if (captured==1 && target==1) manager_.home();
      break;
    }
    case TouchEvent::Type::Cancel: pressed_=tapTarget_=-1; invalidate(Nav); break;
  }
}

void PcMonitorScreen::drawHeader() {
  renderer_.fillRegion(HEADER);
  const PcLinkState s=lastState_;
  renderer_.drawText(monitor_.hasStats()?monitor_.stats().host:"NO PC YET",8,37,2,theme::Mint);
  const uint16_t ink=s==PcLinkState::Connected?theme::Mint:s==PcLinkState::Waiting?theme::Gold:theme::Rose;
  const Rect pill={208,35,104,18};
  renderer_.fillRegion(pill,ink);
  renderer_.drawText(pcLinkName(s),pill.x+pill.w/2,pill.y+5,1,theme::Background,ink,TC_DATUM);
}
void PcMonitorScreen::drawMeter(int16_t y,const char* label,uint16_t permille,const char* value,bool known) {
  renderer_.drawText(label,8,y+3,1,theme::Gold);
  const Rect frame={BAR_X,y,BAR_W,BAR_H};
  renderer_.fillRegion(frame,theme::Panel);
  if (known) {
    const int16_t cells=static_cast<int16_t>(static_cast<uint32_t>(permille>1000?1000:permille)*20/1000);
    for (int16_t i=0;i<20;++i)  // Segmented pixel meter: 20 cells of 9 px.
      renderer_.fillRegion({static_cast<int16_t>(BAR_X+i*9+1),static_cast<int16_t>(y+2),7,BAR_H-4},
                           i<cells?meterColor(permille):theme::Background);
  }
  renderer_.drawText(value,312,y+3,1,known?theme::White:theme::Panel,theme::Background,TR_DATUM);
}
void PcMonitorScreen::drawMeters() {
  renderer_.fillRegion(METERS);
  const bool live=monitor_.hasStats() && lastState_==PcLinkState::Connected;
  const PcStats& s=monitor_.stats();
  char text[24];
  std::snprintf(text,sizeof(text),"%u.%u%%",static_cast<unsigned>(s.cpuPermille/10),static_cast<unsigned>(s.cpuPermille%10));
  drawMeter(62,"CPU",s.cpuPermille,live?text:"--",live);
  std::snprintf(text,sizeof(text),"%lu/%luG",static_cast<unsigned long>((s.ramUsedMB+512)/1024),
                static_cast<unsigned long>((s.ramTotalMB+512)/1024));
  drawMeter(86,"RAM",s.ramPermille,live?text:"--",live);
  std::snprintf(text,sizeof(text),"%u%%",static_cast<unsigned>((s.diskPermille+5)/10));
  drawMeter(110,"DISK",s.diskPermille,live?text:"--",live);
}
void PcMonitorScreen::drawDetails() {
  renderer_.fillRegion(DETAILS);
  if (!monitor_.hasStats() || lastState_!=PcLinkState::Connected) {
    renderer_.drawText(lastState_==PcLinkState::Disconnected?"PC AGENT STOPPED OR LAPTOP OFF":
                       lastState_==PcLinkState::NoToken?"TOKEN NOT READY - CONNECT WIFI":
                       lastState_==PcLinkState::NoNetwork?"WIFI REQUIRED (SERIAL: WIFI)":"START deskos_agent.py ON THE PC",
                       160,138,1,theme::Rose,theme::Background,TC_DATUM);
    return;
  }
  const PcStats& s=monitor_.stats();
  char text[48];
  if (s.upKBps<0 || s.downKBps<0) std::snprintf(text,sizeof(text),"NET N/A");
  else std::snprintf(text,sizeof(text),"NET UP %ld KB/S  DOWN %ld KB/S",static_cast<long>(s.upKBps),static_cast<long>(s.downKBps));
  renderer_.drawText(text,8,136,1,theme::Mint);
  if (s.batteryPercent<0) std::snprintf(text,sizeof(text),"BATTERY N/A (DESKTOP)");
  else std::snprintf(text,sizeof(text),"BATTERY %d%%%s",static_cast<int>(s.batteryPercent),s.charging?" CHARGING":"");
  renderer_.drawText(text,8,150,1,s.batteryPercent>=0 && s.batteryPercent<20 && !s.charging?theme::Rose:theme::Gold);
}
void PcMonitorScreen::drawFooter() {
  renderer_.fillRegion(FOOTER);
  char ip[16],text[48];
  formatIPv4(network_.ipv4(),ip,sizeof(ip));
  if (network_.connected()) std::snprintf(text,sizeof(text),"LISTEN %s UDP %u (LAN ONLY)",ip,static_cast<unsigned>(PC_MONITOR_PORT));
  else std::snprintf(text,sizeof(text),"NOT LISTENING - WIFI OFFLINE");
  renderer_.drawText(text,8,168,1,theme::Mint);
  if (monitor_.hasStats()) std::snprintf(text,sizeof(text),"LAST %lu S AGO  OK %lu  REJ %lu",
                                         static_cast<unsigned long>(monitor_.secondsSinceLast()),
                                         static_cast<unsigned long>(monitor_.accepted()),static_cast<unsigned long>(monitor_.rejected()));
  else std::snprintf(text,sizeof(text),"NO PACKET YET  REJ %lu",static_cast<unsigned long>(monitor_.rejected()));
  renderer_.drawText(text,8,180,1,theme::White);
  if (monitor_.rejected()) {
    std::snprintf(text,sizeof(text),"LAST REJECT: %s",pcRxName(monitor_.lastReject()));
    renderer_.drawText(text,8,190,1,theme::Rose);
  }
}

void PcMonitorScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("PC MONITOR"); regions_=All; }
  if (regions_&Header) drawHeader();
  if (regions_&Meters) drawMeters();
  if (regions_&Details) drawDetails();
  if (regions_&Footer) drawFooter();
  if (regions_&Nav) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==0);
    renderer_.drawButton(layout::Home,"HOME",pressed_==1);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
