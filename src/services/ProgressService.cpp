#include "ProgressService.h"
#include <limits.h>
#include <stdio.h>

namespace deskos {
namespace {
unsigned bitCount(uint32_t value) {
  unsigned count=0;
  for (;value;value&=value-1) ++count;
  return count;
}
}  // namespace

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

RewardLedger ProgressService::emptyLedger(uint32_t openDayId) {
  RewardLedger ledger{};
  ledger.openDayId=openDayId;
  return ledger;
}
bool ProgressService::validLedger(const RewardLedger& l) {
  // Every reward is 1..MAX_SINGLE_REWARD_XP, so the totals bound each other.
  return l.openDayId!=0 && bitCount(l.rewardedMask)<=l.grantedCount &&
         l.grantedCount<=l.bonusXP &&
         l.bonusXP<=static_cast<uint64_t>(l.grantedCount)*MAX_SINGLE_REWARD_XP;
}
bool ProgressService::rewardGranted(const RewardLedger& l,const RewardKey& key) {
  if (key.questId==0 || key.questId>MAX_REWARD_QUEST_ID) return false;
  if (key.dayId!=l.openDayId) return key.dayId<l.openDayId;  // Closed days are final.
  return (l.rewardedMask&(1UL<<(key.questId-1)))!=0;
}
RewardResult ProgressService::grantRewardOnce(RewardLedger& l,const RewardKey& key,uint32_t xp) {
  if (!validLedger(l) || key.questId==0 || key.questId>MAX_REWARD_QUEST_ID ||
      xp==0 || xp>MAX_SINGLE_REWARD_XP || key.dayId==0) return RewardResult::Invalid;
  if (key.dayId<l.openDayId) return RewardResult::DayClosed;
  if (key.dayId>l.openDayId) return RewardResult::Invalid;  // Day must be opened first.
  const uint32_t bit=1UL<<(key.questId-1);
  if (l.rewardedMask&bit) return RewardResult::AlreadyRewarded;
  if (l.bonusXP>UINT32_MAX-xp || l.grantedCount==UINT32_MAX) return RewardResult::LimitReached;
  l.bonusXP+=xp; ++l.grantedCount; l.rewardedMask|=bit;
  return RewardResult::Granted;
}
bool ProgressService::openRewardDay(RewardLedger& l,uint32_t dayId) {
  if (!validLedger(l) || dayId<=l.openDayId) return false;  // Strictly increasing.
  l.openDayId=dayId; l.rewardedMask=0;  // XP and grant count are kept.
  return true;
}
int ProgressService::formatRewardKey(char* out,size_t size,const RewardKey& key) {
  return snprintf(out,size,"mission:%lu:%u",static_cast<unsigned long>(key.dayId),
                  static_cast<unsigned>(key.questId));
}
const char* ProgressService::resultName(RewardResult r) {
  switch (r) {
    case RewardResult::Granted: return "GRANTED";
    case RewardResult::AlreadyRewarded: return "ALREADY_REWARDED";
    case RewardResult::LimitReached: return "LIMIT_REACHED";
    case RewardResult::DayClosed: return "DAY_CLOSED";
    default: return "INVALID";
  }
}
}  // namespace deskos
