#include "FocusQuestScreen.h"
#include <cstdio>

namespace deskos {
namespace {
constexpr Rect PRIMARY={8,152,148,40},RESET={164,152,148,40};
constexpr Rect BAR={16,114,288,14};
constexpr uint32_t CONFIRM_TIMEOUT_MS=5000;
static_assert(PRIMARY.y>=32 && PRIMARY.y+PRIMARY.h<=200 &&
              RESET.x>=PRIMARY.x+PRIMARY.w && RESET.x+RESET.w<=320,"Focus controls overlap/outside content");
}
uint16_t FocusQuestScreen::progressValue() const {
  const uint32_t duration=session_.durationMs(),remaining=session_.remainingMs();
  return static_cast<uint64_t>(duration-remaining)*1000/duration;
}
void FocusQuestScreen::onEnter() {
  pressed_=tapTarget_=-1; confirmReset_=false;
  fullRedraw_=dirty_=true; regions_=All;
  lastSeconds_=session_.remainingSeconds(); lastRevision_=session_.revision();
  lastProgress_=progressValue();
}
void FocusQuestScreen::onExit() {
  // Lifecycle controls UI only. The core owns the timer across all views.
  pressed_=tapTarget_=-1; confirmReset_=false; dirty_=false;
}
void FocusQuestScreen::update(uint32_t deltaMs) {
  (void)deltaMs;
  const uint32_t seconds=session_.remainingSeconds();
  if (seconds!=lastSeconds_) { lastSeconds_=seconds; invalidate(Timer); }
  // Pixel progress changes at most a few times/second; no per-frame redraw.
  const uint16_t progress=progressValue();
  if (progress!=lastProgress_) { lastProgress_=progress; invalidate(Progress); }
  if (lastRevision_!=session_.revision()) {
    lastRevision_=session_.revision(); confirmReset_=false;
    invalidate(Timer|Progress|Details|Controls);
  }
  if (confirmReset_ && session_.nowMs()-confirmAt_>=CONFIRM_TIMEOUT_MS) {
    confirmReset_=false; tapTarget_=-1; pressed_=-1; invalidate(Details|Controls);
  }
}
int FocusQuestScreen::hit(int16_t x,int16_t y) const {
  if (PRIMARY.contains(x,y)) return 0;
  if (RESET.contains(x,y)) return 1;
  if (layout::Back.contains(x,y)) return 2;
  return layout::Home.contains(x,y)?3:-1;
}
void FocusQuestScreen::invalidateTarget(int target) {
  if (target>=0) invalidate(target<2?Controls:Navigation);
}
void FocusQuestScreen::activate(int target) {
  if (target==2) { manager_.back(); return; }
  if (target==3) { manager_.home(); return; }
  if (session_.savePending() || session_.storageReadBlocked()) return;
  // A phase may finish between PRESS and TAP. Never reinterpret PAUSE as START.
  if (pressedState_!=session_.state() || pressedMode_!=session_.mode() ||
      pressedConfirm_!=confirmReset_) return;
  if (confirmReset_) {
    confirmReset_=false;
    if (target==1) session_.reset(); // Left button is CANCEL; right is CONFIRM.
  } else if (target==1) {
    if (session_.state()==FocusState::Running) {
      confirmReset_=true; confirmAt_=session_.nowMs();
    } else session_.reset();
  } else if (target==0) {
    if (session_.state()==FocusState::Running) session_.pause();
    else if (session_.state()==FocusState::Paused) session_.resume();
    else session_.start();
  }
  invalidate(Details|Controls|Timer|Progress);
}
void FocusQuestScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press:
      pressed_=tapTarget_=target;
      pressedState_=session_.state(); pressedMode_=session_.mode(); pressedConfirm_=confirmReset_;
      invalidateTarget(target); break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { invalidateTarget(pressed_); pressed_=tapTarget_=-1; }
      break;
    case TouchEvent::Type::Release:
      invalidateTarget(pressed_); pressed_=-1;
      if (target!=tapTarget_) tapTarget_=-1;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=-1; // Consume before dispatch.
      if (captured>=0 && captured==target) activate(captured);
      break;
    }
    case TouchEvent::Type::Cancel:
      invalidateTarget(pressed_); pressed_=tapTarget_=-1; break;
  }
}
void FocusQuestScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("FOCUS"); }
  if (regions_&Details) {
    renderer_.fillRegion({0,32,320,37});
    renderer_.drawText("FOCUS QUEST",8,38,2,theme::Rose);
    char text[64];
    std::snprintf(text,sizeof(text),"%s / %s",focusModeName(session_.mode()),focusStateName(session_.state()));
    renderer_.drawText(text,312,42,1,theme::Mint,theme::Background,TR_DATUM);
    std::snprintf(text,sizeof(text),"XP %lu   SESSIONS %lu",
                  static_cast<unsigned long>(progress_.totalXP()),
                  static_cast<unsigned long>(progress_.completedFocusSessions()));
    renderer_.drawText(text,8,59,1,theme::Gold);
    renderer_.fillRegion({0,130,320,18});
    const char* message=confirmReset_?"RESET TIMER? XP IS KEPT":
      session_.savePending()?"SAVE PENDING - RETRYING":
      session_.error()!=FocusError::None?focusErrorName(session_.error()):
      session_.state()==FocusState::Completed?"PHASE DONE - PRESS START":
      session_.mode()==FocusMode::Focus?"+25 XP PER COMPLETED FOCUS":"BREAK - NO XP REWARD";
    renderer_.drawText(message,160,134,1,session_.error()==FocusError::None?theme::Mint:theme::Rose,
                       theme::Background,TC_DATUM);
  }
  if (regions_&Timer) {
    renderer_.fillRegion({72,72,176,36});
    char text[12]; const uint32_t seconds=session_.remainingSeconds();
    std::snprintf(text,sizeof(text),"%02lu:%02lu",static_cast<unsigned long>(seconds/60),
                  static_cast<unsigned long>(seconds%60));
    renderer_.drawText(text,160,73,4,theme::Gold,theme::Background,TC_DATUM);
  }
  if (regions_&Progress) renderer_.drawProgressBar(BAR,progressValue());
  if (regions_&Controls) {
    const char* primary=confirmReset_?"CANCEL":session_.state()==FocusState::Running?"PAUSE":
                        session_.state()==FocusState::Paused?"RESUME":"START";
    const bool enabled=!session_.savePending() && !session_.storageReadBlocked();
    renderer_.drawButton(PRIMARY,primary,pressed_==0,enabled);
    renderer_.drawButton(RESET,confirmReset_?"CONFIRM":"RESET",pressed_==1,enabled);
  }
  if (regions_&Navigation) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==2);
    renderer_.drawButton(layout::Home,"HOME",pressed_==3);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
