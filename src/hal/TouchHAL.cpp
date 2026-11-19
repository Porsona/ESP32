#include "TouchHAL.h"
#include "board_pins.h"
#include <stdlib.h>

TouchHAL::TouchHAL() : spi_(VSPI), controller_(PIN_TOUCH_CS, PIN_TOUCH_IRQ) {}

bool TouchHAL::begin() {
  if (initialized_) return true;
  spi_.begin(PIN_TOUCH_SCLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  initialized_ = controller_.begin(spi_);
  controller_.setRotation(1);  // Preserve the tested library rotation, once only.
  // The installed library returns true after setup; this is not a device probe.
  return initialized_;
}

TouchFrame TouchHAL::poll(uint32_t now) {
  TouchFrame frame;
  if (!initialized_ || now - lastPoll_ < 10) return frame;
  lastPoll_ = now;
  frame.updated = true;
  bool rawDown = controller_.touched();
  if (rawDown) {
    TS_Point p = controller_.getPoint();
    RawTouchPoint point;
    point.x = p.x; point.y = p.y; point.z = p.z;
    frame.rawValid = validRawTouch(point);
    if (frame.rawValid) last_ = point;
    rawDown = frame.rawValid;
  }
  if (rawDown != candidate_) { candidate_ = rawDown; candidateAt_ = now; }
  if (candidate_ != down_ && now - candidateAt_ >= 40) {
    down_ = candidate_;
    if (down_) {
      frame.events = TOUCH_PRESS;
      pressedAt_ = now;
      origin_ = last_;
      maxMotion_ = 0;
    } else {
      frame.events = TOUCH_RELEASE;
      frame.heldMs = now - pressedAt_;
      if (frame.heldMs <= 700 && maxMotion_ <= 160 && (!tapSeen_ || now-lastTapAt_>=250)) {
        frame.events |= TOUCH_TAP;
        tapSeen_=true; lastTapAt_=now;
      }
    }
  }
  if (down_ && frame.rawValid) {
    const int dx = abs(last_.x - origin_.x), dy = abs(last_.y - origin_.y);
    if (dx > maxMotion_) maxMotion_ = dx;
    if (dy > maxMotion_) maxMotion_ = dy;
  }
  frame.down = down_;
  frame.raw = last_;  // RELEASE retains last valid position; rawValid remains false.
  if (down_) frame.heldMs = now - pressedAt_;
  return frame;
}
