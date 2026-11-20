#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../services/DailyMissionService.h"

namespace deskos {
class DailyMissionsScreen : public IApp {
 public:
  DailyMissionsScreen(PixelRenderer& renderer,AppManager& manager,
                      DailyMissionService& missions,const ProgressService& progress)
      : renderer_(renderer),manager_(manager),missions_(missions),progress_(progress) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
  uint8_t page() const { return page_; }
  bool confirming() const { return confirming_; }
 private:
  enum Region : uint8_t { Stats=1,Bar=2,Row0=4,Row1=8,Row2=16,Rows=28,Side=32,Nav=64,Dialog=128,All=255 };
  enum Target : int8_t { NoTarget=-1,RowA=0,RowB=1,RowC=2,Prev=3,Next=4,NewDay=5,Confirm=6,Cancel=7,Back=8,Home=9 };
  int hit(int16_t x,int16_t y) const;
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void invalidateTarget(int target);
  void activate(int target);
  void tapQuest(uint8_t slot);
  void closeConfirm();
  void notice(const char* text,uint16_t color);
  uint8_t pageCount() const;
  void drawStats();
  void drawRow(uint8_t row);
  void drawSparkles(uint8_t row,uint8_t frame);
  void drawSide();
  void drawConfirm();
  PixelRenderer& renderer_;
  AppManager& manager_;
  DailyMissionService& missions_;
  const ProgressService& progress_;
  uint8_t regions_=All,page_=0,effectSlot_=0,effectFrame_=0;
  bool fullRedraw_=true,confirming_=false,pressedConfirm_=false,noticeActive_=false,
       cooldown_=false,effect_=false,guard_=false;
  int pressed_=NoTarget,tapTarget_=NoTarget;
  uint32_t pressedDay_=0,confirmDay_=0,lastRevision_=0,lastXP_=0,lastDay_=0,lastMask_=0;
  uint32_t confirmAt_=0,noticeAt_=0,cooldownAt_=0,effectAt_=0,guardAt_=0;
  uint16_t noticeColor_=0;
  char noticeText_[24]={0};
};
}  // namespace deskos
