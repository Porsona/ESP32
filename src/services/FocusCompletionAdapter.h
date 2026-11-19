#pragma once
#include "FocusSessionService.h"
#include "MissionStorage.h"

namespace deskos {
// Adapter only: reads committed M1 state, never touches FocusSessionService.
// lastRewardedSessionId changes only after the Focus +25 XP commit verified.
class FocusCompletionAdapter : public IFocusCompletionSource {
 public:
  FocusCompletionAdapter(const FocusSessionService& session,const ProgressService& progress)
      : session_(session),progress_(progress) {}
  bool available() const override { return !session_.storageReadBlocked(); }
  uint32_t lastCompletedSessionId() const override { return progress_.data().lastRewardedSessionId; }
 private:
  const FocusSessionService& session_;
  const ProgressService& progress_;
};
}  // namespace deskos
