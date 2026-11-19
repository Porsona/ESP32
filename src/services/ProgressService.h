#pragma once
#include <stdint.h>

namespace deskos {
struct ProgressData {
  uint32_t totalXP, completedFocusSessions, currentSessionId, lastRewardedSessionId;
};
enum class RewardResult { Granted, AlreadyRewarded, Invalid, LimitReached };

class FocusSessionService;
class ProgressService {
 public:
  const ProgressData& data() const { return data_; }
  uint32_t totalXP() const { return data_.totalXP; }
  uint32_t completedFocusSessions() const { return data_.completedFocusSessions; }
  static bool valid(const ProgressData& data);
  static bool beginFocus(ProgressData& candidate);
  static RewardResult rewardFocus(ProgressData& candidate,uint32_t sessionId);
 private:
  friend class FocusSessionService;
  void adopt(const ProgressData& committed) { data_=committed; }
  ProgressData data_{};
};
}  // namespace deskos
