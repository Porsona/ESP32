#pragma once
#include <stddef.h>
#include "ProgressService.h"

namespace deskos {
constexpr uint8_t MAX_QUESTS=5;
constexpr uint8_t QUESTS_PER_PAGE=3;
constexpr uint8_t QUEST_TITLE_MAX=16;      // 16 GLCD chars at size 2 = 192 px.
constexpr uint8_t QUEST_TITLE_BYTES=20;    // Zero padded for a deterministic CRC.
constexpr uint8_t DEFAULT_QUEST_XP=10;
constexpr uint16_t MISSION_SCHEMA_VERSION=1;
constexpr uint32_t FOCUS_ANCHOR_UNKNOWN=UINT32_MAX;
constexpr uint32_t MISSION_RETRY_MS=5000;

enum class QuestKind : uint8_t { Manual=0, FocusSession=1 };
constexpr uint8_t QUEST_COMPLETED=1;

struct QuestSlot {
  uint8_t questId, rewardXP;
  QuestKind kind;
  uint8_t flags;
  char title[QUEST_TITLE_BYTES];
};
static_assert(sizeof(QuestSlot)==24,"Change mission schema version when QuestSlot changes");

struct MissionRecord {
  uint32_t magic;
  uint16_t version,bytes;
  uint32_t dayId;          // Manual, strictly increasing quest day (starts at 1).
  uint32_t calendarDay;    // 0 in M2; reserved for a TimeService/NTP day key (M5).
  uint32_t focusAnchorId;  // Focus lastRewardedSessionId when this day opened.
  uint8_t questCount,reserved[3];
  QuestSlot quests[MAX_QUESTS];
  RewardLedger ledger;     // Completion and reward live in one blob: one commit.
  uint32_t generation,checksum;
};
static_assert(sizeof(MissionRecord)==168,"Change mission schema version when record layout changes");

uint32_t missionChecksum(const MissionRecord& record);
void sealMissionRecord(MissionRecord& record);
bool validMissionRecord(const MissionRecord& record);
MissionRecord defaultMissionRecord();
// Uppercases and validates [A-Z0-9 :+-/.!?#], 1..16 chars, no edge spaces.
bool normalizeQuestTitle(const char* input,char (&out)[QUEST_TITLE_BYTES]);
int findQuest(const MissionRecord& record,uint8_t questId);
inline bool questCompleted(const QuestSlot& q) { return (q.flags&QUEST_COMPLETED)!=0; }
bool questRewarded(const MissionRecord& record,uint8_t index);
uint8_t completedQuestCount(const MissionRecord& record);
uint32_t completedQuestMask(const MissionRecord& record);  // Bit per slot index.
uint16_t missionPermille(const MissionRecord& record);
const char* questKindName(QuestKind kind);

// Two NVS slots (A/B). Each commit writes the slot not holding the last good
// record, so one corrupt blob falls back to the previous committed state.
enum class SlotStatus : uint8_t { Missing, Valid, Invalid, IoError };
enum class MissionLoadResult : uint8_t { Missing, Loaded, Recovered, Invalid, IoError };
struct SlotRead { SlotStatus status; MissionRecord record; };
MissionLoadResult selectMissionSlot(const SlotRead (&slots)[2],MissionRecord& out,uint8_t& index);
const char* missionLoadName(MissionLoadResult result);
}  // namespace deskos
