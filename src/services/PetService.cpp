#include "PetService.h"
#include <string.h>

namespace deskos {
namespace { const char* const NS="deskos-pet"; const char* const KEY="state"; }

void PetService::begin() {
  if (initialized_) return;
  initialized_=true;
  record_=defaultPetRecord();
  PetRecord loaded=defaultPetRecord();
  loadResult_=store_.load(NS,KEY,&loaded,sizeof(loaded));
  if (loadResult_==BlobLoad::Loaded && !validPetRecord(loaded)) loadResult_=BlobLoad::Invalid;
  if (loadResult_==BlobLoad::Loaded) record_=loaded;
  // Pet data is cosmetic: a bad record falls back to defaults; XP lives elsewhere.
  saved_=record_;
  if (loadResult_!=BlobLoad::Loaded) memset(&saved_,0,sizeof(saved_));  // Forces one save.
  moodAt_=lastInput_=clock_.nowMs(); ++revision_;
}
void PetService::setMood(PetMood m) {
  if (mood_==m) return;
  mood_=m; moodAt_=clock_.nowMs(); ++revision_;
}
void PetService::raise(PetEvent e,uint32_t value) { event_=e; eventValue_=value; ++revision_; }

void PetService::absorbProgress() {
  const uint32_t current=xp();
  const uint8_t lvl=petLevel(current);
  if (current<record_.lastSeenXP) {  // Shared XP never shrinks normally; resync silently.
    record_.lastSeenXP=current; record_.lastSeenLevel=lvl; return;
  }
  if (current==record_.lastSeenXP) return;
  const uint32_t gained=current-record_.lastSeenXP;
  const uint8_t oldLevel=record_.lastSeenLevel;
  record_.lastSeenXP=current; record_.lastSeenLevel=lvl;
  if (lvl>oldLevel) {
    setMood(PetMood::Celebrate);
    PetStyle unlocked=PetStyle::Count;
    for (uint8_t l=static_cast<uint8_t>(oldLevel+1);l<=lvl && l>oldLevel;++l) {
      const PetStyle s=styleUnlockedAt(l);
      if (s!=PetStyle::Count) unlocked=s;
    }
    if (unlocked!=PetStyle::Count) raise(PetEvent::StyleUnlocked,static_cast<uint8_t>(unlocked));
    else raise(PetEvent::LevelUp,lvl);
  } else {
    if (mood_!=PetMood::Celebrate) setMood(PetMood::Happy);
    raise(PetEvent::XpGained,gained);
  }
  moodAt_=lastInput_=clock_.nowMs();
}

void PetService::onVisit() {
  lastInput_=clock_.nowMs();
  setMood(PetMood::Idle); event_=PetEvent::None; ++revision_;
  absorbProgress();
}
void PetService::update() {
  if (!initialized_) return;
  absorbProgress();  // e.g. a Focus session finishing while the pet is on screen.
  const uint32_t now=clock_.nowMs();
  if (mood_==PetMood::Happy && now-moodAt_>=HAPPY_MS) setMood(PetMood::Idle);
  else if (mood_==PetMood::Celebrate && now-moodAt_>=CELEBRATE_MS) setMood(PetMood::Idle);
  else if (mood_==PetMood::Idle && now-lastInput_>=SLEEP_AFTER_MS) { setMood(PetMood::Sleep); raise(PetEvent::Slept,0); }
}
void PetService::pet() {
  lastInput_=clock_.nowMs();
  const bool wasAsleep=mood_==PetMood::Sleep;
  if (record_.petCount<UINT32_MAX) ++record_.petCount;  // Saved on exit, not per tap.
  if (mood_!=PetMood::Celebrate) { setMood(PetMood::Happy); moodAt_=lastInput_; }
  raise(wasAsleep?PetEvent::Woke:PetEvent::Petted,record_.petCount);
}
void PetService::toggleSleep() {
  lastInput_=clock_.nowMs();
  if (mood_==PetMood::Sleep) { setMood(PetMood::Idle); raise(PetEvent::Woke,0); }
  else { setMood(PetMood::Sleep); raise(PetEvent::Slept,0); }
}
bool PetService::cycleStyle() {
  lastInput_=clock_.nowMs();
  const uint8_t lvl=level();
  for (uint8_t step=1;step<PET_STYLE_COUNT;++step) {
    const PetStyle next=static_cast<PetStyle>((record_.style+step)%PET_STYLE_COUNT);
    if (!styleUnlocked(next,lvl)) continue;
    if (static_cast<uint8_t>(next)==record_.style) break;
    record_.style=static_cast<uint8_t>(next);
    raise(PetEvent::StyleChanged,record_.style);
    persist();  // Explicit setting: save now.
    return true;
  }
  // Only one style unlocked: report the next unlock level.
  for (uint8_t i=1;i<PET_STYLE_COUNT;++i) {
    const PetStyle s=static_cast<PetStyle>(i);
    if (!styleUnlocked(s,lvl)) { raise(PetEvent::StyleLocked,styleUnlockLevel(s)); return false; }
  }
  return false;
}
bool PetService::persist() {
  sealPetRecord(record_);
  if (!memcmp(&record_,&saved_,sizeof(record_))) return true;
  if (!store_.save(NS,KEY,&record_,sizeof(record_))) return false;
  saved_=record_;
  return true;
}
}  // namespace deskos
