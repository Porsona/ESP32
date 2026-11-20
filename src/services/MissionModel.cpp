#include "MissionModel.h"
#include <string.h>
#include "Crc32.h"

namespace deskos {
namespace {
constexpr uint32_t MAGIC=0x44514D31UL;  // "DQM1"
struct Template { uint8_t id; QuestKind kind; const char* title; };
const Template TEMPLATES[MAX_QUESTS]={
  {1,QuestKind::Manual,"STUDY SQL"},
  {2,QuestKind::Manual,"PRACTICE PYTHON"},
  {3,QuestKind::Manual,"READ 10 PAGES"},
  {4,QuestKind::Manual,"REVIEW NOTES"},
  {5,QuestKind::FocusSession,"COMPLETE FOCUS"}
};
bool titleChar(char c) {
  return (c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ' || c==':' || c=='+' ||
         c=='-' || c=='/' || c=='.' || c=='!' || c=='?' || c=='#';
}
bool validStoredTitle(const char (&title)[QUEST_TITLE_BYTES]) {
  size_t length=0;
  while (length<QUEST_TITLE_BYTES && title[length]) ++length;
  if (length==0 || length>QUEST_TITLE_MAX) return false;
  for (size_t i=length;i<QUEST_TITLE_BYTES;++i) if (title[i]) return false;  // Zero padding.
  char normalized[QUEST_TITLE_BYTES];
  return normalizeQuestTitle(title,normalized) && memcmp(normalized,title,QUEST_TITLE_BYTES)==0;
}
}  // namespace

uint32_t missionChecksum(const MissionRecord& r) { return crc32Bytes(&r,offsetof(MissionRecord,checksum)); }
void sealMissionRecord(MissionRecord& r) { r.checksum=missionChecksum(r); }

bool normalizeQuestTitle(const char* input,char (&out)[QUEST_TITLE_BYTES]) {
  memset(out,0,sizeof(out));
  if (!input) return false;
  size_t length=0;
  for (const char* p=input;*p;++p) {
    if (length>=QUEST_TITLE_MAX) return false;
    char c=*p;
    if (c>='a' && c<='z') c=static_cast<char>(c-'a'+'A');
    if (!titleChar(c)) { memset(out,0,sizeof(out)); return false; }
    out[length++]=c;
  }
  if (length==0 || out[0]==' ' || out[length-1]==' ') { memset(out,0,sizeof(out)); return false; }
  return true;
}

MissionRecord defaultMissionRecord() {
  MissionRecord r;
  memset(&r,0,sizeof(r));
  r.magic=MAGIC; r.version=MISSION_SCHEMA_VERSION; r.bytes=sizeof(r);
  r.dayId=1; r.focusAnchorId=FOCUS_ANCHOR_UNKNOWN; r.questCount=MAX_QUESTS;
  for (uint8_t i=0;i<MAX_QUESTS;++i) {
    QuestSlot& q=r.quests[i];
    q.questId=TEMPLATES[i].id; q.rewardXP=DEFAULT_QUEST_XP; q.kind=TEMPLATES[i].kind;
    normalizeQuestTitle(TEMPLATES[i].title,q.title);
  }
  r.ledger=ProgressService::emptyLedger(r.dayId);
  sealMissionRecord(r);
  return r;
}

int findQuest(const MissionRecord& r,uint8_t questId) {
  const uint8_t count=r.questCount>MAX_QUESTS?MAX_QUESTS:r.questCount;
  for (uint8_t i=0;i<count;++i) if (r.quests[i].questId==questId) return i;
  return -1;
}
bool questRewarded(const MissionRecord& r,uint8_t index) {
  if (index>=r.questCount || index>=MAX_QUESTS) return false;
  const RewardKey key={r.dayId,r.quests[index].questId};
  return r.ledger.openDayId==r.dayId && ProgressService::rewardGranted(r.ledger,key);
}
uint8_t completedQuestCount(const MissionRecord& r) {
  uint8_t count=0;
  for (uint8_t i=0;i<r.questCount && i<MAX_QUESTS;++i) if (questCompleted(r.quests[i])) ++count;
  return count;
}
uint32_t completedQuestMask(const MissionRecord& r) {
  uint32_t mask=0;
  for (uint8_t i=0;i<r.questCount && i<MAX_QUESTS;++i) if (questCompleted(r.quests[i])) mask|=1UL<<i;
  return mask;
}
uint16_t missionPermille(const MissionRecord& r) {
  if (!r.questCount || r.questCount>MAX_QUESTS) return 0;
  return static_cast<uint16_t>(static_cast<uint32_t>(completedQuestCount(r))*1000/r.questCount);
}

bool validMissionRecord(const MissionRecord& r) {
  if (r.magic!=MAGIC || r.version!=MISSION_SCHEMA_VERSION || r.bytes!=sizeof(r) ||
      r.reserved[0] || r.reserved[1] || r.reserved[2] || r.checksum!=missionChecksum(r)) return false;
  if (r.dayId==0 || r.questCount==0 || r.questCount>MAX_QUESTS) return false;
  if (!ProgressService::validLedger(r.ledger) || r.ledger.openDayId!=r.dayId) return false;
  uint32_t ids=0;
  for (uint8_t i=0;i<MAX_QUESTS;++i) {
    const QuestSlot& q=r.quests[i];
    if (i>=r.questCount) {  // Unused slots stay all-zero.
      const QuestSlot zero={0,0,QuestKind::Manual,0,{0}};
      if (memcmp(&q,&zero,sizeof(q))) return false;
      continue;
    }
    if (q.questId==0 || q.questId>MAX_REWARD_QUEST_ID || (ids&(1UL<<(q.questId-1)))) return false;
    ids|=1UL<<(q.questId-1);
    if (q.rewardXP==0 || q.rewardXP>MAX_SINGLE_REWARD_XP ||
        static_cast<uint8_t>(q.kind)>1 || (q.flags&~QUEST_COMPLETED)) return false;
    if (!validStoredTitle(q.title)) return false;
    // Reward without completion is impossible: both change in one candidate.
    if (questRewarded(r,i) && !questCompleted(q)) return false;
  }
  return (r.ledger.rewardedMask&~ids)==0;
}

MissionLoadResult selectMissionSlot(const SlotRead (&slots)[2],MissionRecord& out,uint8_t& index) {
  bool invalid=false;
  int best=-1;
  for (int i=0;i<2;++i) {
    // An unreadable slot may hold the newest commit: never guess past it.
    if (slots[i].status==SlotStatus::IoError) return MissionLoadResult::IoError;
    if (slots[i].status==SlotStatus::Invalid ||
        (slots[i].status==SlotStatus::Valid && !validMissionRecord(slots[i].record))) { invalid=true; continue; }
    if (slots[i].status!=SlotStatus::Valid) continue;
    if (best<0 || slots[i].record.generation>slots[best].record.generation) best=i;
  }
  if (best<0) return invalid?MissionLoadResult::Invalid:MissionLoadResult::Missing;
  out=slots[best].record; index=static_cast<uint8_t>(best);
  return invalid?MissionLoadResult::Recovered:MissionLoadResult::Loaded;
}
const char* missionLoadName(MissionLoadResult r) {
  switch (r) {
    case MissionLoadResult::Missing: return "MISSING";
    case MissionLoadResult::Loaded: return "LOADED";
    case MissionLoadResult::Recovered: return "RECOVERED_PREVIOUS_SLOT";
    case MissionLoadResult::Invalid: return "INVALID";
    default: return "IO_ERROR";
  }
}
const char* questKindName(QuestKind k) { return k==QuestKind::FocusSession?"FOCUS":"MANUAL"; }
}  // namespace deskos
