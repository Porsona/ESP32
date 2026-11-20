#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../services/NetTypes.h"
#include "../services/PcMonitorService.h"

namespace deskos {
class PcMonitorScreen : public IApp {
 public:
  PcMonitorScreen(PixelRenderer& renderer,AppManager& manager,const PcMonitorService& monitor,
                  const INetworkStatus& network,IFocusClock& clock)
      : renderer_(renderer),manager_(manager),monitor_(monitor),network_(network),clock_(clock) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
  PcLinkState shownState() const { return lastState_; }
 private:
  enum Region : uint8_t { Header=1,Meters=2,Details=4,Footer=8,Nav=16,All=31 };
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void drawHeader();
  void drawMeter(int16_t y,const char* label,uint16_t permille,const char* value,bool known);
  void drawMeters();
  void drawDetails();
  void drawFooter();
  PixelRenderer& renderer_;
  AppManager& manager_;
  const PcMonitorService& monitor_;
  const INetworkStatus& network_;
  IFocusClock& clock_;
  uint8_t regions_=All;
  bool fullRedraw_=true;
  int pressed_=-1,tapTarget_=-1;
  uint32_t lastRevision_=0,lastSecond_=0;
  PcLinkState lastState_=PcLinkState::NoNetwork;
};
}  // namespace deskos
