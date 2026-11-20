#pragma once
#include "FocusStorage.h"
#include "MissionStorage.h"

namespace deskos {
enum class MissionError : uint8_t { None, InvalidRestore, StorageRead, StorageWrite, DayLimit, RewardLimit, InvalidTransition };
enum class QuestResult : uint8_t { Completed, AlreadyCompleted, FocusLinked, NotFound, Blocked, SavePending, Failed };
enum class DayResult : uint8_t { Started, Stale, Blocked, SavePending, Limit, Failed };
const char* missionErrorName(MissionError error);
const char* questResultName(QuestResult result);
const char* dayResultName(DayResult result);

class DailyMissionService {
 public:
  // Call begin() after FocusSessionService::begin() so Focus progress is loaded.
  DailyMissionService(IFocusClock& clock,IMissionStorage& storage,ProgressService& progress,
                      IFocusCompletionSource& focus,const IDaySource* days=nullptr)
      : clock_(clock),storage_(storage),progress_(progress),focus_(focus),days_(days) {}
  void begin();
  void update();
  QuestResult completeQuest(uint8_t questId);
  // expectedDayId makes one confirmation advance at most one day.
  DayResult startNewDay(uint32_t expectedDayId);
  bool setTitle(uint8_t questId,const char* title);

  uint32_t dayId() const { return visible().dayId; }
  uint8_t questCount() const { return visible().questCount; }
  const QuestSlot& quest(uint8_t index) const { return visible().quests[index<MAX_QUESTS?index:0]; }
  bool rewarded(uint8_t index) const { return questRewarded(visible(),index); }
  uint8_t completedCount() const { return completedQuestCount(visible()); }
  uint32_t completedMask() const { return completedQuestMask(visible()); }
  uint16_t progressPermille() const { return missionPermille(visible()); }
  const MissionRecord& committed() const { return record_; }
  uint32_t revision() const { return revision_; }
  uint32_t nowMs() const { return clock_.nowMs(); }
  bool savePending() const { return pendingCommit_; }
  bool storageReadBlocked() const { return loadResult_==MissionLoadResult::IoError; }
  bool blocked() const { return !initialized_ || pendingCommit_ || storageReadBlocked(); }
  MissionLoadResult loadResult() const { return loadResult_; }
  MissionError error() const { return error_; }
  RewardResult lastReward() const { return lastReward_; }
  RewardKey lastRewardKey() const { return lastRewardKey_; }
 private:
  enum class Commit : uint8_t { Committed, Pending, Rejected };
  const MissionRecord& visible() const { return pendingCommit_?pending_:record_; }
  uint32_t currentFocusAnchor() const;
  bool applyCompletion(MissionRecord& candidate,uint8_t index);
  Commit commit(MissionRecord candidate);
  bool retry(uint32_t now);
  void checkFocusCompletion();
  void syncCalendarDay();
  DayResult openDay(uint32_t expectedDayId,uint32_t calendarDay);
  void setError(MissionError error);
  IFocusClock& clock_;
  IMissionStorage& storage_;
  ProgressService& progress_;
  IFocusCompletionSource& focus_;
  const IDaySource* days_;
  MissionRecord record_{},pending_{};
  bool initialized_=false,pendingCommit_=false;
  uint32_t lastRetry_=0,revision_=0,ignoredFocusId_=0;
  MissionLoadResult loadResult_=MissionLoadResult::Missing;
  MissionError error_=MissionError::None;
  RewardResult lastReward_=RewardResult::Invalid;
  RewardKey lastRewardKey_{0,0};
};
}  // namespace deskos
