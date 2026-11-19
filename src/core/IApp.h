#pragma once
#include "AppTypes.h"

namespace deskos {
class IApp {
 public:
  virtual ~IApp() = default;
  virtual void onEnter() = 0;
  virtual void update(uint32_t deltaMs) = 0;
  virtual void render() = 0;
  virtual void onTouch(const TouchEvent& event) = 0;
  virtual void onExit() = 0;
  bool needsRender() const { return dirty_; }
 protected:
  bool dirty_=true;
};
}  // namespace deskos
