#include "DailyMissionService.h"
#include <string.h>

namespace deskos {
const char* missionErrorName(MissionError e) {
  switch (e) {
    case MissionError::None: return "NONE";
    case MissionError::InvalidRestore: return "INVALID_RESTORE";
    case MissionError::StorageRead: return "STORAGE_READ";
    case MissionError::StorageWrite: return "STORAGE_WRITE";
    case MissionError::DayLimit: return "DAY_ID_LIMIT";
    case MissionError::RewardLimit: return "REWARD_LIMIT";
    default: return "INVALID_TRANSITION";
  }
}
const char* questResultName(QuestResult r) {
  switch (r) {
    case QuestResult::Completed: return "COMPLETED";
    case QuestResult::AlreadyCompleted: return "ALREADY_COMPLETED";
    case QuestResult::FocusLinked: return "FOCUS_LINKED";
    case QuestResult::NotFound: return "NOT_FOUND";
    case QuestResult::Blocked: return "BLOCKED";
    case QuestResult::SavePending: return "SAVE_PENDING";
    default: return "FAILED";
  }
}
const char* dayResultName(DayResult r) {
  switch (r) {
    case DayResult::Started: return "STARTED";
    case DayResult::Stale: return "STALE_REQUEST";
    case DayResult::Blocked: return "BLOCKED";
    case DayResult::SavePending: return "SAVE_PENDING";
    case DayResult::Limit: return "DAY_ID_LIMIT";
    default: return "FAILED";
  }
}

void DailyMissionService::setError(MissionError e) {
  if (error_!=e) { error_=e; ++revision_; }
}
uint32_t DailyMissionService::currentFocusAnchor() const {
  return focus_.available()?focus_.lastCompletedSessionId():FOCUS_ANCHOR_UNKNOWN;
}

void DailyMissionService::begin() {
  if (initialized_) return;
  initialized_=true; record_=defaultMissionRecord();
  MissionRecord loaded=defaultMissionRecord();
  loadResult_=storage_.load(loaded);
  const bool gotRecord=loadResult_==MissionLoadResult::Loaded || loadResult_==MissionLoadResult::Recovered;
  if (gotRecord && !validMissionRecord(loaded)) loadResult_=MissionLoadResult::Invalid;
  lastRetry_=clock_.nowMs();
  switch (loadResult_) {
    case MissionLoadResult::Loaded:
    case MissionLoadResult::Recovered:
      record_=loaded; break;
    case MissionLoadResult::IoError:
      error_=MissionError::StorageRead; break;  // Block writes: never replace unread XP.
    case MissionLoadResult::Invalid:
      // Keep the bad blob until a user action commits; do not guess lost XP.
      error_=MissionError::InvalidRestore;
      record_.focusAnchorId=currentFocusAnchor(); sealMissionRecord(record_); break;
    case MissionLoadResult::Missing:
      record_.focusAnchorId=currentFocusAnchor(); sealMissionRecord(record_);
      progress_.adoptLedger(record_.ledger);
      // One-time initial write pins the Focus anchor for reboot recovery.
      commit(record_);
      ++revision_;
      return;
  }
  progress_.adoptLedger(record_.ledger);
  ++revision_;
}

bool DailyMissionService::retry(uint32_t now) {
  lastRetry_=now;
  // The cached candidate is retried byte-for-byte; rewards are never recomputed.
  if (!storage_.save(pending_)) { setError(MissionError::StorageWrite); return false; }
  record_=pending_; pendingCommit_=false;
  progress_.adoptLedger(record_.ledger);  // XP becomes visible only after commit.
  error_=MissionError::None; ++revision_;
  return true;
}

DailyMissionService::Commit DailyMissionService::commit(MissionRecord candidate) {
  candidate.generation=record_.generation+1; sealMissionRecord(candidate);
  if (!validMissionRecord(candidate)) { setError(MissionError::InvalidTransition); return Commit::Rejected; }
  pending_=candidate; pendingCommit_=true; ++revision_;
  return retry(clock_.nowMs())?Commit::Committed:Commit::Pending;
}

bool DailyMissionService::applyCompletion(MissionRecord& candidate,uint8_t index) {
  QuestSlot& q=candidate.quests[index];
  q.flags|=QUEST_COMPLETED;
  lastRewardKey_.dayId=candidate.dayId; lastRewardKey_.questId=q.questId;
  lastReward_=ProgressService::grantRewardOnce(candidate.ledger,lastRewardKey_,q.rewardXP);
  if (lastReward_==RewardResult::Invalid || lastReward_==RewardResult::DayClosed) {
    setError(MissionError::InvalidTransition); return false;
  }
  if (lastReward_==RewardResult::LimitReached) setError(MissionError::RewardLimit);
  return true;  // Granted, or completed without a second grant.
}

QuestResult DailyMissionService::completeQuest(uint8_t questId) {
  if (blocked()) return pendingCommit_?QuestResult::SavePending:QuestResult::Blocked;
  const int index=findQuest(record_,questId);
  if (index<0) return QuestResult::NotFound;
  const QuestSlot& q=record_.quests[index];
  if (questCompleted(q)) return QuestResult::AlreadyCompleted;  // No write, no XP.
  if (q.kind==QuestKind::FocusSession) return QuestResult::FocusLinked;
  MissionRecord candidate=record_;
  if (!applyCompletion(candidate,static_cast<uint8_t>(index))) return QuestResult::Failed;
  switch (commit(candidate)) {
    case Commit::Committed: return QuestResult::Completed;
    case Commit::Pending: return QuestResult::SavePending;
    default: return QuestResult::Failed;
  }
}

void DailyMissionService::checkFocusCompletion() {
  if (!focus_.available()) return;
  const uint32_t id=focus_.lastCompletedSessionId();
  if (record_.focusAnchorId==FOCUS_ANCHOR_UNKNOWN || id<record_.focusAnchorId) {
    // Unknown or regressed Focus progress (e.g. invalid Focus restore): re-anchor
    // in RAM without completing anything; the next commit persists it.
    record_.focusAnchorId=id; sealMissionRecord(record_); return;
  }
  if (id==record_.focusAnchorId || id==ignoredFocusId_) return;
  for (uint8_t i=0;i<record_.questCount;++i) {
    const QuestSlot& q=record_.quests[i];
    if (q.kind!=QuestKind::FocusSession || questCompleted(q)) continue;
    MissionRecord candidate=record_;
    if (!applyCompletion(candidate,i) || commit(candidate)==Commit::Rejected) ignoredFocusId_=id;
    return;  // One transaction per update; a pending commit blocks the next.
  }
}

void DailyMissionService::syncCalendarDay() {
  uint32_t key=0;
  if (!days_ || !days_->calendarDay(key) || key==0) return;
  if (record_.calendarDay==0) {
    MissionRecord candidate=record_; candidate.calendarDay=key;  // First valid clock: stamp only.
    commit(candidate); return;
  }
  if (key>record_.calendarDay) openDay(record_.dayId,key);  // Clock going backwards is ignored.
}

DayResult DailyMissionService::openDay(uint32_t expectedDayId,uint32_t calendarDay) {
  if (blocked()) return pendingCommit_?DayResult::SavePending:DayResult::Blocked;
  if (expectedDayId!=record_.dayId) return DayResult::Stale;
  if (record_.dayId==UINT32_MAX) { setError(MissionError::DayLimit); return DayResult::Limit; }
  MissionRecord candidate=record_;
  ++candidate.dayId;
  for (uint8_t i=0;i<candidate.questCount;++i) candidate.quests[i].flags&=static_cast<uint8_t>(~QUEST_COMPLETED);
  // XP and grant count stay; only the new day's keys become grantable.
  if (!ProgressService::openRewardDay(candidate.ledger,candidate.dayId)) {
    setError(MissionError::InvalidTransition); return DayResult::Failed;
  }
  candidate.focusAnchorId=currentFocusAnchor();  // Earlier Focus sessions do not count.
  if (calendarDay) candidate.calendarDay=calendarDay;
  ignoredFocusId_=0;
  switch (commit(candidate)) {
    case Commit::Committed: return DayResult::Started;
    case Commit::Pending: return DayResult::SavePending;
    default: return DayResult::Failed;
  }
}

DayResult DailyMissionService::startNewDay(uint32_t expectedDayId) { return openDay(expectedDayId,0); }

bool DailyMissionService::setTitle(uint8_t questId,const char* title) {
  if (blocked()) return false;
  const int index=findQuest(record_,questId);
  char normalized[QUEST_TITLE_BYTES];
  if (index<0 || !normalizeQuestTitle(title,normalized)) return false;
  if (!memcmp(normalized,record_.quests[index].title,sizeof(normalized))) return true;
  MissionRecord candidate=record_;
  memcpy(candidate.quests[index].title,normalized,sizeof(normalized));  // questId/reward key unchanged.
  return commit(candidate)!=Commit::Rejected;
}

void DailyMissionService::update() {
  if (!initialized_) return;
  const uint32_t now=clock_.nowMs();
  if (pendingCommit_) {
    if (now-lastRetry_>=MISSION_RETRY_MS) retry(now);
    return;
  }
  if (storageReadBlocked()) return;
  syncCalendarDay();
  checkFocusCompletion();
}
}  // namespace deskos
