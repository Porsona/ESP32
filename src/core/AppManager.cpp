#include "AppManager.h"
#include <Arduino.h>

namespace deskos {
namespace {
const AppInfo CATALOG[APP_COUNT]={
  {AppId::Home,"DeskOS Home","HOME"},
  {AppId::Pet,"Pixel Pet","PET"},
  {AppId::Window,"Pixel Window","WINDOW"},
  {AppId::Focus,"Focus Quest","FOCUS"},
  {AppId::Missions,"Daily Missions","MISSIONS"},
  {AppId::PcMonitor,"PC Monitor","PC MON"},
  {AppId::Weather,"Living Weather","WEATHER"}
};
bool valid(AppId id) { return static_cast<uint8_t>(id)<APP_COUNT; }
}

const AppInfo& appInfo(AppId id) { return CATALOG[valid(id)?static_cast<uint8_t>(id):0]; }

bool AppManager::registerApp(AppId id,IApp& app) {
  if (!valid(id) || registry_[static_cast<uint8_t>(id)]) return false;
  registry_[static_cast<uint8_t>(id)]=&app;
  return true;
}

void AppManager::open(AppId id) {
  if (!valid(id) || !registry_[static_cast<uint8_t>(id)]) return;
  pendingId_=id;  // Defer lifecycle changes until update, outside an input callback.
}

void AppManager::cancelTouch() {
  if (active_) active_->onTouch({TouchEvent::Type::Cancel,0,0,0,0});
  contact_=tapArmed_=false; cancelled_=true;
}

void AppManager::suspend() {
  cancelTouch();
  if (active_) active_->onExit();
  active_=nullptr; activeId_=pendingId_=AppId::None; logTransition_=false;
}

void AppManager::checkMotion(const TouchEvent& event) {
  const int32_t dx=event.x-originX_,dy=event.y-originY_;
  if (!cancelled_ && (dx*dx+dy*dy>64 || event.heldMs>700)) {
    cancelled_=true; tapArmed_=false;
    active_->onTouch({TouchEvent::Type::Cancel,event.x,event.y,event.timestampMs,event.heldMs});
  }
}

void AppManager::onTouch(const TouchEvent& event) {
  if (!active_ || pendingId_!=AppId::None) return;
  switch (event.type) {
    case TouchEvent::Type::Press:
      contact_=true; cancelled_=tapArmed_=false;
      originX_=event.x; originY_=event.y;
      active_->onTouch(event);
      break;
    case TouchEvent::Type::Move:
      if (contact_) {
        checkMotion(event);
        if (!cancelled_) active_->onTouch(event);
      }
      break;
    case TouchEvent::Type::Release:
      if (contact_) {
        checkMotion(event); contact_=false; tapArmed_=!cancelled_;
        active_->onTouch(event);
      }
      break;
    case TouchEvent::Type::Tap:
      if (tapArmed_ && !cancelled_) { tapArmed_=false; active_->onTouch(event); }
      break;
    case TouchEvent::Type::Cancel: cancelTouch(); break;
  }
}

void AppManager::applyPending() {
  if (pendingId_==AppId::None) return;
  const AppId next=pendingId_; pendingId_=AppId::None;
  if (next==activeId_) return;
  transitionAtUs_=micros();
  cancelTouch();
  if (active_) active_->onExit();
  activeId_=next; active_=registry_[static_cast<uint8_t>(next)];
  active_->onEnter();
  ++switchCount_; logTransition_=true;
}

void AppManager::update(uint32_t deltaMs) {
  applyPending();
  if (!active_) return;
  active_->update(deltaMs);
  if (active_->needsRender()) active_->render();
  if (logTransition_) {
    const uint32_t duration=micros()-transitionAtUs_;
    logTransition_=false;
    Serial.printf("APP_SWITCH: id=%u app=%s duration_us=%lu switches=%lu free_heap=%u largest_free_block=%u\n",
                  static_cast<unsigned>(activeId_),appInfo(activeId_).label,
                  static_cast<unsigned long>(duration),static_cast<unsigned long>(switchCount_),
                  ESP.getFreeHeap(),ESP.getMaxAllocHeap());
  }
}
}  // namespace deskos
