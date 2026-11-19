#include "ProgressService.h"
#include <limits.h>

namespace deskos {
bool ProgressService::valid(const ProgressData& p) {
  return p.lastRewardedSessionId<=p.currentSessionId &&
         p.completedFocusSessions<=p.lastRewardedSessionId &&
         (p.completedFocusSessions!=0 || p.lastRewardedSessionId==0) &&
         static_cast<uint64_t>(p.completedFocusSessions)*25<=p.totalXP;
}
bool ProgressService::beginFocus(ProgressData& p) {
  if (!valid(p) || p.currentSessionId==UINT32_MAX) return false;
  ++p.currentSessionId;
  return true;
}
RewardResult ProgressService::rewardFocus(ProgressData& p,uint32_t id) {
  if (!valid(p) || id==0) return RewardResult::Invalid;
  if (id<=p.lastRewardedSessionId) return RewardResult::AlreadyRewarded;
  if (id!=p.currentSessionId) return RewardResult::Invalid;
  if (p.totalXP>UINT32_MAX-25 || p.completedFocusSessions==UINT32_MAX) return RewardResult::LimitReached;
  p.totalXP+=25; ++p.completedFocusSessions; p.lastRewardedSessionId=id;
  return RewardResult::Granted;
}
}  // namespace deskos
