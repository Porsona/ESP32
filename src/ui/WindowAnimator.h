#pragma once
#include "PixelRenderer.h"

namespace deskos {
enum class WindowScene : uint8_t { Day, Night, Rain, Auto, Count };
enum class WindowSpeed : uint8_t { Slow, Normal, Fast, Count };
const char* sceneName(WindowScene scene);
const char* speedName(WindowSpeed speed);
uint16_t speedPeriodMs(WindowSpeed speed);

namespace window {
constexpr Rect FRAME={8,36,240,160};
constexpr Rect GLASS={12,40,232,152};
constexpr Rect PANES[4]={{12,40,114,74},{130,40,114,74},{12,118,114,74},{130,118,114,74}};
constexpr Rect MULLION_V={126,40,4,152},MULLION_H={12,114,232,4};
}  // namespace window

// Lightweight scene animation inside the window glass. State is a few small
// fixed arrays; every animated pixel is drawn through paneFill(), clipped to the
// four glass panes, and erased by restoring the sky band colour underneath.
class WindowAnimator {
 public:
  explicit WindowAnimator(PixelRenderer& renderer) : renderer_(renderer) {}
  void drawFrame();                 // Wooden frame + mullions (static).
  void start(WindowScene resolved); // Full glass redraw of a concrete scene.
  void step();                      // One animation step (cheap, bounded).
  WindowScene scene() const { return scene_; }
  uint32_t steps() const { return steps_; }
  uint16_t lastStepFills() const { return lastFills_; }
 private:
  struct Cloud { int16_t x,y; uint8_t every; };
  struct Star { int16_t x,y; uint8_t level; };
  struct Drop { int16_t x,y; uint8_t speed; };
  static constexpr uint8_t CLOUDS=3,STARS=28,DROPS=36;
  uint32_t random();
  uint16_t sky(int16_t y) const;
  void paneFill(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t color);
  void restore(int16_t x,int16_t y,int16_t w,int16_t h);
  void drawHills();
  void drawDisc(int16_t cx,int16_t cy,int16_t r,uint16_t color,bool skyColor=false);
  void drawRing(int16_t cx,int16_t cy,int16_t inner,int16_t outer,uint16_t color,bool skyColor);
  void drawCloud(const Cloud& c,bool full);
  void drawRainClouds(bool flash);
  void resetDrop(Drop& d,bool anywhere);
  PixelRenderer& renderer_;
  WindowScene scene_=WindowScene::Day;
  Cloud clouds_[CLOUDS]{};
  Star stars_[STARS]{};
  Drop drops_[DROPS]{};
  uint32_t seed_=0x2545F491UL,steps_=0;
  uint8_t glowPhase_=0,flash_=0;
  uint16_t fills_=0,lastFills_=0;
};
}  // namespace deskos
