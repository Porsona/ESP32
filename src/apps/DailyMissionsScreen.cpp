#include "DailyMissionsScreen.h"
#include <cstdio>
#include <cstring>

namespace deskos {
namespace {
// Status y=0..31, progress y=32..57, quest list y=58..199, navigation y=200..239.
constexpr Rect STATS={0,32,320,13};
constexpr Rect BAR={8,46,304,10};
constexpr Rect ROWS[QUESTS_PER_PAGE]={{8,60,242,44},{8,106,242,44},{8,152,242,44}};
constexpr Rect PREV={258,60,54,44},NEXT={258,106,54,44},NEW_DAY={258,152,54,44};
constexpr Rect DIALOG={8,60,304,136};
// CONFIRM sits far from NEW DAY; a repeated tap at NEW DAY's spot lands on CANCEL.
constexpr Rect CONFIRM={16,148,136,40},CANCEL={168,148,136,40};
constexpr uint32_t NOTICE_MS=2000,CONFIRM_TIMEOUT_MS=5000,CONFIRM_ARM_MS=300;
constexpr uint32_t NEW_DAY_COOLDOWN_MS=3000,EFFECT_FRAME_MS=100,DIALOG_GUARD_MS=500;
constexpr uint8_t EFFECT_FRAMES=6;

constexpr bool apart(const Rect& a,const Rect& b) {
  return a.x+a.w<=b.x || b.x+b.w<=a.x || a.y+a.h<=b.y || b.y+b.h<=a.y;
}
constexpr bool inside(const Rect& r,int16_t top,int16_t bottom) {
  return r.x>=0 && r.x+r.w<=320 && r.y>=top && r.y+r.h<=bottom;
}
static_assert(inside(STATS,32,58) && inside(BAR,32,58),"Progress area is y=32..57");
static_assert(inside(ROWS[0],58,200) && inside(ROWS[2],58,200) && inside(NEW_DAY,58,200),"Quest list is y=58..199");
static_assert(apart(ROWS[0],ROWS[1]) && apart(ROWS[1],ROWS[2]) && apart(ROWS[0],PREV) &&
              apart(ROWS[1],NEXT) && apart(ROWS[2],NEW_DAY) && apart(PREV,NEXT) && apart(NEXT,NEW_DAY),
              "Quest hitboxes overlap");
static_assert(apart(CONFIRM,CANCEL) && apart(CONFIRM,NEW_DAY) && !apart(CANCEL,NEW_DAY),
              "CONFIRM must not share NEW DAY's spot");
static_assert(ROWS[0].h>=40 && PREV.w>=40 && PREV.h>=40 && CONFIRM.h>=40,"Hitboxes must be >=40 px");
}  // namespace

uint8_t DailyMissionsScreen::pageCount() const {
  const uint8_t count=missions_.questCount();
  return count?static_cast<uint8_t>((count+QUESTS_PER_PAGE-1)/QUESTS_PER_PAGE):1;
}
void DailyMissionsScreen::onEnter() {
  // Page is kept across visits; quest state always comes from the service.
  if (page_>=pageCount()) page_=0;
  pressed_=tapTarget_=NoTarget; confirming_=noticeActive_=effect_=guard_=false;
  fullRedraw_=dirty_=true; regions_=All;
  lastRevision_=missions_.revision(); lastXP_=progress_.totalXP();
  lastDay_=missions_.dayId(); lastMask_=completedQuestMask(missions_.committed());
}
void DailyMissionsScreen::onExit() {
  pressed_=tapTarget_=NoTarget; confirming_=effect_=false; dirty_=false;
}
void DailyMissionsScreen::notice(const char* text,uint16_t color) {
  std::snprintf(noticeText_,sizeof(noticeText_),"%s",text);
  noticeColor_=color; noticeActive_=true; noticeAt_=missions_.nowMs(); invalidate(Stats);
}
void DailyMissionsScreen::closeConfirm() {
  if (!confirming_) return;
  // Rows reappear under CONFIRM/CANCEL: a quick extra tap must not hit a quest.
  confirming_=false; guard_=true; guardAt_=missions_.nowMs(); invalidate(Rows|Side|Stats);
}

void DailyMissionsScreen::update(uint32_t deltaMs) {
  (void)deltaMs;
  const uint32_t now=missions_.nowMs();
  if (lastRevision_!=missions_.revision()) {
    lastRevision_=missions_.revision();
    const uint32_t mask=completedQuestMask(missions_.committed());
    const uint32_t gained=mask&~lastMask_;
    if (missions_.dayId()!=lastDay_) {
      lastDay_=missions_.dayId(); closeConfirm();  // A day change cancels any open prompt.
    } else if (gained) {
      // Celebrate committed completions only (manual tap or Focus-linked).
      uint8_t slot=0;
      while (!(gained&(1UL<<slot))) ++slot;
      char text[24];
      std::snprintf(text,sizeof(text),"+%u XP! QUEST DONE",static_cast<unsigned>(missions_.quest(slot).rewardXP));
      notice(text,theme::Gold);
      if (slot/QUESTS_PER_PAGE==page_) { effect_=true; effectSlot_=slot; effectFrame_=0; effectAt_=now; }
    }
    lastMask_=mask;
    invalidate(confirming_?Stats|Bar:Stats|Bar|Rows|Side);
  }
  if (progress_.totalXP()!=lastXP_) { lastXP_=progress_.totalXP(); invalidate(Stats); }
  if (noticeActive_ && now-noticeAt_>=NOTICE_MS) { noticeActive_=false; invalidate(Stats); }
  if (confirming_ && now-confirmAt_>=CONFIRM_TIMEOUT_MS) {
    closeConfirm(); pressed_=tapTarget_=NoTarget; notice("NEW DAY CANCELLED",theme::Rose);
  }
  if (cooldown_ && now-cooldownAt_>=NEW_DAY_COOLDOWN_MS) { cooldown_=false; invalidate(Side); }
  if (effect_) {
    const uint32_t frame=(now-effectAt_)/EFFECT_FRAME_MS;
    if (frame!=effectFrame_) {
      effectFrame_=static_cast<uint8_t>(frame>EFFECT_FRAMES?EFFECT_FRAMES:frame);
      if (effectFrame_>=EFFECT_FRAMES) effect_=false;  // Final redraw clears sparkles.
      if (effectSlot_/QUESTS_PER_PAGE==page_ && !confirming_) invalidate(Row0<<(effectSlot_%QUESTS_PER_PAGE));
      else effect_=false;
    }
  }
}

int DailyMissionsScreen::hit(int16_t x,int16_t y) const {
  if (layout::Back.contains(x,y)) return Back;
  if (layout::Home.contains(x,y)) return Home;
  if (confirming_) {
    if (CONFIRM.contains(x,y)) return Confirm;
    return CANCEL.contains(x,y)?Cancel:NoTarget;
  }
  for (uint8_t row=0;row<QUESTS_PER_PAGE;++row) {
    if (ROWS[row].contains(x,y)) return page_*QUESTS_PER_PAGE+row<missions_.questCount()?static_cast<int>(row):-1;
  }
  if (PREV.contains(x,y)) return Prev;
  if (NEXT.contains(x,y)) return Next;
  return NEW_DAY.contains(x,y)?NewDay:NoTarget;
}
void DailyMissionsScreen::invalidateTarget(int target) {
  if (target<0) return;
  if (target<=RowC) invalidate(Row0<<target);
  else if (target<=NewDay) invalidate(Side);
  else if (target<=Cancel) invalidate(Dialog);
  else invalidate(Nav);
}

void DailyMissionsScreen::tapQuest(uint8_t slot) {
  const QuestSlot& quest=missions_.quest(slot);
  switch (missions_.completeQuest(quest.questId)) {
    case QuestResult::Completed: break;  // Effect + notice come from the committed revision.
    case QuestResult::AlreadyCompleted: notice("DONE - XP CLAIMED",theme::Mint); break;
    case QuestResult::FocusLinked: notice("FINISH A 25:00 FOCUS",theme::Rose); break;
    case QuestResult::SavePending: notice("SAVE PENDING - RETRY",theme::Rose); break;
    case QuestResult::Blocked: notice("STORAGE READ ERROR",theme::Rose); break;
    default: notice("QUEST NOT SAVED",theme::Rose); break;
  }
}

void DailyMissionsScreen::activate(int target) {
  if (target==Back) { manager_.back(); return; }
  if (target==Home) { manager_.home(); return; }
  // Meaning is fixed at PRESS: a prompt/day change before TAP voids the gesture.
  if (pressedConfirm_!=confirming_ || pressedDay_!=missions_.dayId()) return;
  const uint32_t now=missions_.nowMs();
  if (confirming_) {
    if (target==Cancel) { closeConfirm(); notice("NEW DAY CANCELLED",theme::Mint); return; }
    if (target!=Confirm || now-confirmAt_<CONFIRM_ARM_MS) return;
    const DayResult result=missions_.startNewDay(confirmDay_);
    closeConfirm();
    if (result==DayResult::Started || result==DayResult::SavePending) {
      page_=0; cooldown_=true; cooldownAt_=now;
      char text[24];
      std::snprintf(text,sizeof(text),"DAY %lu STARTED",static_cast<unsigned long>(missions_.dayId()));
      notice(result==DayResult::Started?text:"SAVE PENDING - RETRY",
             result==DayResult::Started?theme::Gold:theme::Rose);
    } else notice(result==DayResult::Stale?"DAY ALREADY CHANGED":"NEW DAY FAILED",theme::Rose);
    return;
  }
  if (guard_ && now-guardAt_<DIALOG_GUARD_MS) return;
  guard_=false;
  if (target<=RowC) { tapQuest(static_cast<uint8_t>(page_*QUESTS_PER_PAGE+target)); return; }
  if (target==Prev || target==Next) {
    const uint8_t pages=pageCount();
    if (target==Prev && page_>0) --page_;
    else if (target==Next && page_+1<pages) ++page_;
    else return;
    effect_=false; invalidate(Rows|Side|Stats);
    return;
  }
  if (target==NewDay) {
    if (missions_.blocked()) { notice(missions_.savePending()?"SAVE PENDING - RETRY":"STORAGE READ ERROR",theme::Rose); return; }
    if (cooldown_) { notice("WAIT - DAY JUST BEGAN",theme::Rose); return; }
    confirming_=true; confirmAt_=now; confirmDay_=missions_.dayId(); effect_=false;
    invalidate(Dialog|Stats);
  }
}

void DailyMissionsScreen::onTouch(const TouchEvent& event) {
  const int target=hit(event.x,event.y);
  switch (event.type) {
    case TouchEvent::Type::Press:
      pressed_=tapTarget_=target; pressedConfirm_=confirming_; pressedDay_=missions_.dayId();
      invalidateTarget(target); break;
    case TouchEvent::Type::Move:
      if (target!=tapTarget_) { invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget; }
      break;
    case TouchEvent::Type::Release:
      invalidateTarget(pressed_); pressed_=NoTarget;
      if (target!=tapTarget_) tapTarget_=NoTarget;
      break;
    case TouchEvent::Type::Tap: {
      const int captured=tapTarget_; tapTarget_=NoTarget;  // Consume before dispatch: one action per TAP.
      if (captured>=0 && captured==target) activate(captured);
      break;
    }
    case TouchEvent::Type::Cancel:
      invalidateTarget(pressed_); pressed_=tapTarget_=NoTarget; break;
  }
}

void DailyMissionsScreen::drawStats() {
  renderer_.fillRegion(STATS);
  char text[32];
  std::snprintf(text,sizeof(text),"DAY %lu",static_cast<unsigned long>(missions_.dayId()));
  renderer_.drawText(text,8,35,1,theme::Mint);
  std::snprintf(text,sizeof(text),"XP %lu",static_cast<unsigned long>(progress_.totalXP()));
  renderer_.drawText(text,312,35,1,theme::Gold,theme::Background,TR_DATUM);
  const char* center=text;
  uint16_t ink=theme::Mint;
  if (noticeActive_) { center=noticeText_; ink=noticeColor_; }
  else if (missions_.savePending()) { center="SAVE PENDING - RETRY"; ink=theme::Rose; }
  else if (missions_.error()!=MissionError::None) { center=missionErrorName(missions_.error()); ink=theme::Rose; }
  else if (confirming_) center="CONFIRM NEW DAY?";
  else std::snprintf(text,sizeof(text),"%u/%u DONE  PAGE %u/%u",
                     static_cast<unsigned>(missions_.completedCount()),static_cast<unsigned>(missions_.questCount()),
                     static_cast<unsigned>(page_+1),static_cast<unsigned>(pageCount()));
  renderer_.drawText(center,160,35,1,ink,theme::Background,TC_DATUM);
}

void DailyMissionsScreen::drawSparkles(uint8_t row,uint8_t frame) {
  // Expanding 8-point pixel burst around the checkbox, inside the row bounds.
  const Rect& r=ROWS[row];
  const int16_t cx=r.x+22,cy=r.y+22,d=static_cast<int16_t>(15+frame);
  const uint16_t ink=frame%2?theme::Rose:theme::Gold;
  const int16_t points[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
  for (const auto& p:points) {
    const int16_t size=static_cast<int16_t>((p[0]&&p[1])?2:3);
    const int16_t x=cx+p[0]*d-size/2,y=cy+p[1]*(d>19?19:d)-size/2;
    renderer_.fillRegion({x,y,size,size},ink);
  }
}

void DailyMissionsScreen::drawRow(uint8_t row) {
  const Rect& r=ROWS[row];
  const uint8_t slot=static_cast<uint8_t>(page_*QUESTS_PER_PAGE+row);
  renderer_.fillRegion(r);
  if (slot>=missions_.questCount()) return;
  const QuestSlot& quest=missions_.quest(slot);
  const bool done=questCompleted(quest),pressed=pressed_==row,enabled=!missions_.blocked();
  renderer_.drawPanel(r,pressed,enabled);
  const uint16_t fill=!enabled?theme::Background:pressed?theme::Mint:theme::Panel;
  renderer_.drawCheckbox({static_cast<int16_t>(r.x+8),static_cast<int16_t>(r.y+8),28,28},done);
  renderer_.drawText(quest.title,r.x+44,r.y+7,2,pressed?theme::Background:done?theme::Mint:theme::Gold,fill);
  char text[32];
  const unsigned xp=quest.rewardXP;
  const bool celebrating=effect_ && effectSlot_==slot;
  if (done) std::snprintf(text,sizeof(text),missions_.rewarded(slot)?"DONE  +%u XP CLAIMED":"DONE  (NO XP)",xp);
  else if (quest.kind==QuestKind::FocusSession) std::snprintf(text,sizeof(text),"AUTO ON FOCUS  +%u XP",xp);
  else std::snprintf(text,sizeof(text),"TAP TO COMPLETE  +%u XP",xp);
  renderer_.drawText(text,r.x+44,r.y+29,1,pressed?theme::Background:celebrating?theme::Rose:theme::Mint,fill);
  if (celebrating && effectFrame_<EFFECT_FRAMES) drawSparkles(row,effectFrame_);
}

void DailyMissionsScreen::drawSide() {
  const uint8_t pages=pageCount();
  renderer_.drawButton(PREV,"<",pressed_==Prev,page_>0);
  renderer_.drawButton(NEXT,">",pressed_==Next,page_+1<pages);
  renderer_.drawButton(NEW_DAY,"NEW DAY",pressed_==NewDay,!missions_.blocked() && !cooldown_,1);
}

void DailyMissionsScreen::drawConfirm() {
  renderer_.fillRegion({0,58,320,142});
  renderer_.drawPanel(DIALOG);
  char text[32];
  std::snprintf(text,sizeof(text),"START DAY %lu?",static_cast<unsigned long>(confirmDay_+1));
  renderer_.drawText(text,160,70,2,theme::Rose,theme::Panel,TC_DATUM);
  renderer_.drawText("ALL QUEST CHECKS RESET",160,96,1,theme::Mint,theme::Panel,TC_DATUM);
  renderer_.drawText("XP, FOCUS, PET, CALIBRATION KEPT",160,110,1,theme::Mint,theme::Panel,TC_DATUM);
  renderer_.drawText("AUTO-CANCEL IN 5 SECONDS",160,124,1,theme::Gold,theme::Panel,TC_DATUM);
  renderer_.drawButton(CONFIRM,"CONFIRM",pressed_==Confirm);
  renderer_.drawButton(CANCEL,"CANCEL",pressed_==Cancel);
}

void DailyMissionsScreen::render() {
  if (fullRedraw_) { renderer_.clear(); renderer_.drawStatusBar("DAILY QUESTS"); regions_=All; }
  if (regions_&Stats) drawStats();
  if (regions_&Bar) renderer_.drawProgressBar(BAR,missions_.progressPermille());
  if (confirming_) {
    if (regions_&Dialog) drawConfirm();
  } else {
    for (uint8_t row=0;row<QUESTS_PER_PAGE;++row) if (regions_&(Row0<<row)) drawRow(row);
    if (regions_&Side) drawSide();
  }
  if (regions_&Nav) {
    renderer_.drawButton(layout::Back,"BACK",pressed_==Back);
    renderer_.drawButton(layout::Home,"HOME",pressed_==Home);
  }
  fullRedraw_=dirty_=false; regions_=0;
}
}  // namespace deskos
