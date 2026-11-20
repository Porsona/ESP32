#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../ui/WindowAnimator.h"
#include "../services/WindowSettings.h"

namespace deskos {
class PixelWindowScreen : public IApp {
 public:
  PixelWindowScreen(PixelRenderer& renderer,AppManager& manager,IBlobStore& store,const IWindowContext& context)
      : renderer_(renderer),manager_(manager),store_(store),context_(context),animator_(renderer) {}
  void begin();     // Load settings once at boot.
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
  WindowScene selected() const { return static_cast<WindowScene>(settings_.scene); }
  WindowScene shown() const { return animator_.scene(); }
  WindowSpeed speed() const { return static_cast<WindowSpeed>(settings_.speed); }
  uint32_t steps() const { return animator_.steps(); }
  uint16_t lastStepFills() const { return animator_.lastStepFills(); }
  BlobLoad loadResult() const { return loadResult_; }
 private:
  enum Region : uint8_t { Frame=1,Glass=2,Side=4,Nav=8,All=15 };
  enum Target : int8_t { NoTarget=-1,GlassTap=0,SceneButton=1,SpeedButton=2,Back=3,Home=4 };
  WindowScene resolve() const;
  int hit(int16_t x,int16_t y) const;
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void activate(int target);
  void drawSide();
  PixelRenderer& renderer_;
  AppManager& manager_;
  IBlobStore& store_;
  const IWindowContext& context_;
  WindowAnimator animator_;
  WindowSettingsRecord settings_{},saved_{};
  BlobLoad loadResult_=BlobLoad::Missing;
  uint8_t regions_=All;
  bool fullRedraw_=true,animating_=false;
  int pressed_=NoTarget,tapTarget_=NoTarget;
  uint32_t accumulator_=0;
};
}  // namespace deskos
