#pragma once
#include "BlobStore.h"
#include "FocusStorage.h"
#include "PetModel.h"
#include "ProgressService.h"

namespace deskos {
enum class PetEvent : uint8_t { None, XpGained, LevelUp, StyleUnlocked, Petted, Woke, Slept, StyleChanged, StyleLocked,
                                Tickled, Grapple, Hanging };

// Read-only view of shared XP; cosmetic state only. Saves when leaving the
// app or on an explicit style change, never per frame or per tap.
class PetService {
 public:
  static constexpr uint32_t HAPPY_MS=2500,CELEBRATE_MS=3500,SLEEP_AFTER_MS=45000;
  static constexpr uint32_t REACTION_MS=1800,HANG_MS=4000;
  PetService(IBlobStore& store,const ProgressService& progress,IFocusClock& clock)
      : store_(store),progress_(progress),clock_(clock) {}
  void begin();
  void onVisit();   // Screen enter: celebrate XP/levels earned since last visit.
  void update();    // Mood timers and live XP changes while visible.
  void pet();       // TAP on the head or a stroke across the pet.
  void react(PetReaction reaction);  // Tickle / grapple shot / hang; wakes a sleeping pet.
  void toggleSleep();
  bool cycleStyle();
  bool persist();   // Screen exit: write only when something changed.

  uint32_t xp() const { return progress_.totalXP(); }
  uint8_t level() const { return petLevel(xp()); }
  uint16_t levelProgress() const { return levelPermille(xp()); }
  uint32_t nextLevelXP() const { return levelStartXP(static_cast<uint8_t>(level()+1)); }
  PetMood mood() const { return mood_; }
  PetStyle style() const { return static_cast<PetStyle>(record_.style); }
  uint32_t petCount() const { return record_.petCount; }
  PetEvent event() const { return event_; }
  uint32_t eventValue() const { return eventValue_; }
  uint32_t revision() const { return revision_; }
  uint32_t moodSinceMs() const { return moodAt_; }
  PetReaction reaction() const { return reaction_; }
  uint32_t reactionSinceMs() const { return reactionAt_; }
  uint32_t nowMs() const { return clock_.nowMs(); }
  BlobLoad loadResult() const { return loadResult_; }
  const PetRecord& record() const { return record_; }
 private:
  void setMood(PetMood mood);
  void raise(PetEvent event,uint32_t value);
  void absorbProgress();
  IBlobStore& store_;
  const ProgressService& progress_;
  IFocusClock& clock_;
  PetRecord record_{},saved_{};
  void clearReaction();
  PetMood mood_=PetMood::Idle;
  PetReaction reaction_=PetReaction::None;
  PetEvent event_=PetEvent::None;
  uint32_t eventValue_=0,revision_=0,moodAt_=0,lastInput_=0,reactionAt_=0;
  bool initialized_=false;
  BlobLoad loadResult_=BlobLoad::Missing;
};
}  // namespace deskos
