#include "FocusSessionService.h"

namespace deskos {
const char* focusErrorName(FocusError error) {
  switch (error) {
    case FocusError::None: return "NONE";
    case FocusError::InvalidRestore: return "INVALID_RESTORE";
    case FocusError::StorageRead: return "STORAGE_READ";
    case FocusError::StorageWrite: return "STORAGE_WRITE";
    case FocusError::SessionLimit: return "SESSION_ID_LIMIT";
    case FocusError::ProgressLimit: return "PROGRESS_LIMIT";
    default: return "INVALID_TRANSITION";
  }
}
void FocusSessionService::setError(FocusError e) {
  if (error_!=e) { error_=e; ++revision_; }
}
void FocusSessionService::begin() {
  if (initialized_) return;
  initialized_=true; record_=defaultFocusRecord();
  FocusRecord loaded{};
  loadResult_=storage_.load(loaded);
  if (loadResult_==FocusLoadResult::Loaded && !validFocusRecord(loaded)) loadResult_=FocusLoadResult::Invalid;
  if (loadResult_==FocusLoadResult::Loaded) {
    record_=loaded;
    if (record_.state==FocusState::Running) {
      restoredRunning_=true; record_.state=FocusState::Paused;
      sealFocusRecord(record_);  // RAM normalization only; do not write at every boot.
    }
  } else if (loadResult_==FocusLoadResult::Invalid) error_=FocusError::InvalidRestore;
  else if (loadResult_==FocusLoadResult::IoError) error_=FocusError::StorageRead;
  progress_.adopt(record_.progress);
  anchorMs_=lastCheckpointAttempt_=clock_.nowMs(); ++revision_;
}
uint32_t FocusSessionService::remainingAt(uint32_t now) const {
  if (record_.state!=FocusState::Running) return record_.remainingMs;
  const uint32_t elapsed=now-anchorMs_;
  return elapsed>=record_.remainingMs?0:record_.remainingMs-elapsed;
}
uint32_t FocusSessionService::remainingMs() const {
  return pendingCompletion_?pending_.remainingMs:remainingAt(clock_.nowMs());
}
void FocusSessionService::adopt(const FocusRecord& r,uint32_t now) {
  record_=r; progress_.adopt(r.progress); anchorMs_=lastCheckpointAttempt_=now;
  error_=FocusError::None; ++revision_;
}
bool FocusSessionService::commit(FocusRecord candidate,uint32_t now) {
  candidate.generation=record_.generation+1; sealFocusRecord(candidate);
  if (!validFocusRecord(candidate)) { setError(FocusError::InvalidTransition); return false; }
  if (!storage_.save(candidate)) { setError(FocusError::StorageWrite); return false; }
  adopt(candidate,now); return true;
}
void FocusSessionService::retryCompletion(uint32_t now) {
  lastRetry_=now;
  if (!storage_.save(pending_)) { setError(FocusError::StorageWrite); return; }
  const FocusRecord committed=pending_;
  pendingCompletion_=false; adopt(committed,now);
}
void FocusSessionService::complete(uint32_t now) {
  FocusRecord candidate=record_;
  if (record_.mode==FocusMode::Focus) {
    const RewardResult reward=ProgressService::rewardFocus(candidate.progress,candidate.progress.currentSessionId);
    if (reward==RewardResult::Invalid || reward==RewardResult::LimitReached) {
      setError(FocusError::ProgressLimit); return;
    }
  }
  candidate.mode=record_.mode==FocusMode::Focus?FocusMode::Break:FocusMode::Focus;
  // COMPLETED means the previous phase ended; the next mode waits for START.
  candidate.state=FocusState::Completed; candidate.remainingMs=focusDuration(candidate.mode);
  candidate.generation=record_.generation+1; sealFocusRecord(candidate);
  if (!validFocusRecord(candidate)) { setError(FocusError::InvalidTransition); return; }
  pending_=candidate; pendingCompletion_=true; ++revision_;
  // Cache this exact transaction. Failed/ambiguous writes retry it, never re-award.
  retryCompletion(now);
}
void FocusSessionService::advance(uint32_t now,bool checkpoint) {
  if (pendingCompletion_) {
    if (now-lastRetry_>=COMPLETION_RETRY_MS) retryCompletion(now);
    return;
  }
  if (record_.state!=FocusState::Running) return;
  const uint32_t remaining=remainingAt(now);
  if (!remaining) { complete(now); return; }
  if (checkpoint && now-lastCheckpointAttempt_>=FOCUS_CHECKPOINT_MS) {
    lastCheckpointAttempt_=now;
    FocusRecord candidate=record_; candidate.remainingMs=remaining;
    // On failure leave the original anchor intact; RAM time continues accurately.
    commit(candidate,now);
  }
}
void FocusSessionService::update() { if (initialized_) advance(clock_.nowMs(),true); }
bool FocusSessionService::start() {
  if (!initialized_ || storageReadBlocked() || pendingCompletion_ ||
      (record_.state!=FocusState::Idle && record_.state!=FocusState::Completed)) return false;
  FocusRecord candidate=record_;
  if (candidate.mode==FocusMode::Focus && !ProgressService::beginFocus(candidate.progress)) {
    setError(FocusError::SessionLimit); return false;
  }
  candidate.state=FocusState::Running;
  return commit(candidate,clock_.nowMs());
}
bool FocusSessionService::pause() {
  if (!initialized_) return false;
  const uint32_t now=clock_.nowMs(); advance(now,false);
  if (pendingCompletion_ || record_.state!=FocusState::Running) return false;
  FocusRecord candidate=record_; candidate.remainingMs=remainingAt(now); candidate.state=FocusState::Paused;
  return commit(candidate,now);
}
bool FocusSessionService::resume() {
  if (!initialized_ || pendingCompletion_ || record_.state!=FocusState::Paused) return false;
  FocusRecord candidate=record_; candidate.state=FocusState::Running;
  return commit(candidate,clock_.nowMs());
}
bool FocusSessionService::reset() {
  if (!initialized_ || storageReadBlocked()) return false;
  const uint32_t now=clock_.nowMs(); advance(now,false);
  if (pendingCompletion_) return false;
  FocusRecord candidate=record_;
  candidate.mode=FocusMode::Focus; candidate.state=FocusState::Idle; candidate.remainingMs=FOCUS_DURATION_MS;
  return commit(candidate,now);  // XP/count/retired session IDs are preserved.
}
}  // namespace deskos
