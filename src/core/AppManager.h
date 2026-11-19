#pragma once
#include "IApp.h"

namespace deskos {
class AppManager {
 public:
  bool registerApp(AppId id,IApp& app);
  void open(AppId id);
  void home() { open(AppId::Home); }
  void back() { home(); }  // One-level Shell: every placeholder's parent is Home.
  void update(uint32_t deltaMs);
  void onTouch(const TouchEvent& event);
  void cancelTouch();
  void suspend();
  AppId activeId() const { return activeId_; }
  uint32_t switchCount() const { return switchCount_; }
 private:
  void applyPending();
  void checkMotion(const TouchEvent& event);
  IApp* registry_[APP_COUNT]{};
  IApp* active_=nullptr;
  AppId activeId_=AppId::None, pendingId_=AppId::None;
  bool contact_=false, cancelled_=false, tapArmed_=false;
  int16_t originX_=0, originY_=0;
  uint32_t transitionAtUs_=0, switchCount_=0;
  bool logTransition_=false;
};
}  // namespace deskos
