#pragma once
#include "FocusStorage.h"

namespace deskos {
enum class FocusError { None, InvalidRestore, StorageRead, StorageWrite, SessionLimit, ProgressLimit, InvalidTransition };
const char* focusErrorName(FocusError error);
class FocusSessionService {
 public:
  FocusSessionService(IFocusClock& clock,IFocusStorage& storage,ProgressService& progress)
      : clock_(clock),storage_(storage),progress_(progress) {}
  void begin();
  void update();
  bool start();
  bool pause();
  bool resume();
  bool reset();
  FocusMode mode() const { return visibleRecord().mode; }
  FocusState state() const { return visibleRecord().state; }
  uint32_t remainingMs() const;
  uint32_t remainingSeconds() const { return (remainingMs()+999)/1000; }
  uint32_t durationMs() const { return focusDuration(mode()); }
  uint32_t nowMs() const { return clock_.nowMs(); }
  uint32_t revision() const { return revision_; }
  bool savePending() const { return pendingCompletion_; }
  bool storageReadBlocked() const { return loadResult_==FocusLoadResult::IoError; }
  bool restoredRunning() const { return restoredRunning_; }
  FocusLoadResult loadResult() const { return loadResult_; }
  FocusError error() const { return error_; }
 private:
  const FocusRecord& visibleRecord() const { return pendingCompletion_?pending_:record_; }
  uint32_t remainingAt(uint32_t now) const;
  void advance(uint32_t now,bool checkpoint);
  void complete(uint32_t now);
  void retryCompletion(uint32_t now);
  bool commit(FocusRecord candidate,uint32_t now);
  void adopt(const FocusRecord& committed,uint32_t now);
  void setError(FocusError error);
  IFocusClock& clock_;
  IFocusStorage& storage_;
  ProgressService& progress_;
  FocusRecord record_{},pending_{};
  bool initialized_=false,pendingCompletion_=false,restoredRunning_=false;
  uint32_t anchorMs_=0,lastCheckpointAttempt_=0,lastRetry_=0,revision_=0;
  FocusLoadResult loadResult_=FocusLoadResult::Missing;
  FocusError error_=FocusError::None;
};
}  // namespace deskos
