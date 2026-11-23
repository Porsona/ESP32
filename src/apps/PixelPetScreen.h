#pragma once
#include "../core/AppManager.h"
#include "../ui/PixelRenderer.h"
#include "../ui/PetSprites.h"
#include "../services/PetService.h"

namespace deskos {
class PixelPetScreen : public IApp {
 public:
  PixelPetScreen(PixelRenderer& renderer,AppManager& manager,PetService& pet)
      : renderer_(renderer),manager_(manager),pet_(pet) {}
  void onEnter() override;
  void update(uint32_t deltaMs) override;
  void render() override;
  void onTouch(const TouchEvent& event) override;
  void onExit() override;
 private:
  enum Region : uint8_t { Header=1,Info=2,Sprite=4,Extras=8,Side=16,Message=32,Nav=64,All=127 };
  enum Target : int8_t { NoTarget=-1,PetBody=0,Style=1,Sleep=2,Back=3,Home=4 };
  int hit(int16_t x,int16_t y) const;
  void invalidate(uint8_t regions) { regions_|=regions; dirty_=true; }
  void invalidateTarget(int target);
  void activate(int target);
  PetFrame currentFrame() const;
  void interact(int16_t x,int16_t y);
  uint8_t frameKey() const;
  uint8_t extrasKey() const;
  void drawHeader();
  void drawInfo();
  void drawSprite();
  void drawExtras();
  void drawMessage();
  PixelRenderer& renderer_;
  AppManager& manager_;
  PetService& pet_;
  uint8_t regions_=All,lastFrame_=255,lastExtras_=255;
  bool fullRedraw_=true;
  int pressed_=NoTarget,tapTarget_=NoTarget;
  int16_t pressX_=0,pressY_=0,tapX_=0,tapY_=0;
  uint32_t lastRevision_=0,lastXP_=0;
  PetCanvas canvas_;  // 2.3 KB static composition buffer: no heap, no stack spike in render.
};
}  // namespace deskos
