#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Focus part: persisted inside the M1 Focus record (unchanged layout).
struct ProgressData {
  uint32_t totalXP, completedFocusSessions, currentSessionId, lastRewardedSessionId;
};
// Non-Focus reward ledger (M2). Persisted atomically by the reward source's own
// record (Daily Missions) so completion state and XP can never be split.
// Dedup key "mission:<dayId>:<questId>": days before openDayId are closed for
// good; on openDayId, bit (questId-1) of rewardedMask marks a granted key.
struct RewardLedger {
  uint32_t bonusXP, grantedCount, openDayId, rewardedMask;
};
struct RewardKey { uint32_t dayId; uint8_t questId; };
constexpr uint8_t MAX_REWARD_QUEST_ID=32;
constexpr uint32_t MAX_SINGLE_REWARD_XP=100;
// DayClosed is appended so the M1 values keep their meaning.
enum class RewardResult { Granted, AlreadyRewarded, Invalid, LimitReached, DayClosed };

class FocusSessionService;
class DailyMissionService;
class ProgressService {
 public:
  const ProgressData& data() const { return data_; }
  const RewardLedger& ledger() const { return ledger_; }
  uint32_t focusXP() const { return data_.totalXP; }
  uint32_t bonusXP() const { return ledger_.bonusXP; }
  // Shared XP shown by every app: committed Focus XP + committed mission XP.
  uint32_t totalXP() const {
    return ledger_.bonusXP>UINT32_MAX-data_.totalXP?UINT32_MAX:data_.totalXP+ledger_.bonusXP;
  }
  uint32_t completedFocusSessions() const { return data_.completedFocusSessions; }
  static bool valid(const ProgressData& data);
  static bool beginFocus(ProgressData& candidate);
  static RewardResult rewardFocus(ProgressData& candidate,uint32_t sessionId);
  static RewardLedger emptyLedger(uint32_t openDayId);
  static bool validLedger(const RewardLedger& ledger);
  static bool rewardGranted(const RewardLedger& ledger,const RewardKey& key);
  // Pure transactions on a candidate; owners commit to NVS then adopt.
  static RewardResult grantRewardOnce(RewardLedger& candidate,const RewardKey& key,uint32_t xp);
  static bool openRewardDay(RewardLedger& candidate,uint32_t dayId);
  static int formatRewardKey(char* out,size_t size,const RewardKey& key);
  static const char* resultName(RewardResult result);
 private:
  friend class FocusSessionService;
  friend class DailyMissionService;
  friend struct ProgressTestAccess;  // Defined only by host tests.
  void adopt(const ProgressData& committed) { data_=committed; }
  void adoptLedger(const RewardLedger& committed) { ledger_=committed; }
  ProgressData data_{};
  RewardLedger ledger_{};
};
}  // namespace deskos
