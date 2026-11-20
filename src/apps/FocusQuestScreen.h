#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../services/FocusSessionService.h"

namespace deskos {
class FocusQuestScreen : public IApp {
 public:
  FocusQuestScreen(PixelRenderer& renderer,AppManager& manager,
                   FocusSessionService& session,const ProgressService& progress)
      : renderer_(renderer),manager_(manager),session_(session),progress_(progress) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
 private:
  enum Region : uint8_t { Timer=1,Progress=2,Details=4,Controls=8,Navigation=16,All=31 };
  int hit(int16_t x,int16_t y) const;
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void invalidateTarget(int target);
  void activate(int target);
  uint16_t progressValue() const;
  PixelRenderer& renderer_;
  AppManager& manager_;
  FocusSessionService& session_;
  const ProgressService& progress_;
  uint8_t regions_=All;
  bool fullRedraw_=true,confirmReset_=false,pressedConfirm_=false;
  int pressed_=-1,tapTarget_=-1;
  FocusState pressedState_=FocusState::Idle;
  FocusMode pressedMode_=FocusMode::Focus;
  uint32_t lastSeconds_=0,lastRevision_=0,confirmAt_=0;
  uint16_t lastProgress_=0;
};
}  // namespace deskos
