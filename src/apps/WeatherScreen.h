#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../ui/WeatherSprites.h"
#include "../services/WeatherService.h"

namespace deskos {
class WeatherScreen : public IApp {
 public:
  WeatherScreen(PixelRenderer& renderer,AppManager& manager,WeatherService& weather,
                const INetworkStatus& network,const ITimeSource& time,IFocusClock& clock)
      : renderer_(renderer),manager_(manager),weather_(weather),network_(network),time_(time),clock_(clock) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
 private:
  enum Region : uint8_t { Header=1,Icon=2,Info=4,Footer=8,Button=16,Nav=32,All=63 };
  enum Target : int8_t { NoTarget=-1,Refresh=0,Back=1,Home=2 };
  int hit(int16_t x,int16_t y) const;
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void drawHeader();
  void drawIcon();
  void drawInfo();
  void drawFooter();
  PixelRenderer& renderer_;
  AppManager& manager_;
  WeatherService& weather_;
  const INetworkStatus& network_;
  const ITimeSource& time_;
  IFocusClock& clock_;
  uint8_t regions_=All,frame_=0;
  bool fullRedraw_=true;
  int pressed_=NoTarget,tapTarget_=NoTarget;
  uint32_t lastRevision_=0,lastMinuteKey_=0,lastInfoTick_=0,noticeAt_=0;
  NetState lastNet_=NetState::Off;
  const char* notice_=nullptr;
};
}  // namespace deskos
