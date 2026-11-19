#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"

namespace deskos {
class PlaceholderScreen : public IApp {
 public:
  PlaceholderScreen(PixelRenderer& renderer,AppManager& manager) : renderer_(renderer),manager_(manager) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override { (void)deltaMs; }
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
 private:
  int hit(int16_t x,int16_t y) const;
  void invalidate(int region);
  PixelRenderer& renderer_;
  AppManager& manager_;
  AppId id_=AppId::Pet;
  bool fullRedraw_=true;
  uint8_t dirtyRegions_=3;
  int pressed_=-1,tapTarget_=-1;
};
}  // namespace deskos
