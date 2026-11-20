#include <cstdio>
#include <cstring>
#include <climits>
#include "services/DailyMissionService.h"
#include "services/FocusCompletionAdapter.h"

using namespace deskos;
namespace {
int failures=0;
#define CHECK(expression) do { if (!(expression)) { std::printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expression); ++failures; return; } } while (0)

struct FakeClock : IFocusClock {
  uint32_t time=0;
  uint32_t nowMs() const override { return time; }
};
struct FakeFocusStorage : IFocusStorage {
  FocusRecord slot{};
  bool present=false,fail=false;
  unsigned attempts=0;
  FocusLoadResult load(FocusRecord& r) override {
    if (!present) return FocusLoadResult::Missing;
    r=slot; return FocusLoadResult::Loaded;
  }
  bool save(const FocusRecord& r) override {
    ++attempts;
    if (fail || !validFocusRecord(r)) return false;
    slot=r; present=true; return true;
  }
};
// Models the NVS contract: optional failure before commit, commit-then-report-
// failure (ambiguous) and read I/O errors. Service must validate loads itself.
struct FakeMissionStorage : IMissionStorage {
  MissionRecord slot{};
  bool present=false,fail=false,commitThenFail=false,readError=false,raw=false;
  unsigned attempts=0,writes=0;
  MissionLoadResult load(MissionRecord& r) override {
    if (readError) return MissionLoadResult::IoError;
    if (!present) return MissionLoadResult::Missing;
    r=slot; return MissionLoadResult::Loaded;
  }
  bool save(const MissionRecord& r) override {
    ++attempts;
    if (!validMissionRecord(r)) return false;
    if (fail && !commitThenFail) return false;
    slot=r; present=true; ++writes;
    return !fail;
  }
};
struct FakeDays : IDaySource {
  uint32_t day=0;
  bool calendarDay(uint32_t& key) const override { key=day; return day!=0; }
};

// One boot of the device: shared storage objects survive a "reboot".
struct Boot {
  ProgressService progress;
  FocusSessionService focus;
  FocusCompletionAdapter adapter;
  DailyMissionService missions;
  Boot(FakeClock& clock,FakeFocusStorage& fs,FakeMissionStorage& ms,const IDaySource* days=nullptr)
      : focus(clock,fs,progress),adapter(focus,progress),missions(clock,ms,progress,adapter,days) {
    focus.begin(); missions.begin();  // Same order as main.cpp.
  }
  void update() { focus.update(); missions.update(); }
};
struct Fixture {
  FakeClock clock; FakeFocusStorage focusStore; FakeMissionStorage missionStore;
  Boot* boot;
  Fixture() : boot(new Boot(clock,focusStore,missionStore)) {}
  ~Fixture() { delete boot; }
  void reboot(uint32_t elapsed=1000) { delete boot; clock.time+=elapsed; boot=new Boot(clock,focusStore,missionStore); }
  DailyMissionService& m() { return boot->missions; }
  ProgressService& p() { return boot->progress; }
  void runFocus() {
    boot->focus.start(); clock.time+=FOCUS_DURATION_MS; boot->update();
  }
};

void defaultsLoaded() {  // T1
  Fixture f;
  CHECK(f.m().loadResult()==MissionLoadResult::Missing && f.m().dayId()==1);
  CHECK(f.m().questCount()==5 && f.m().completedCount()==0 && f.m().progressPermille()==0);
  const char* titles[5]={"STUDY SQL","PRACTICE PYTHON","READ 10 PAGES","REVIEW NOTES","COMPLETE FOCUS"};
  for (uint8_t i=0;i<5;++i) {
    CHECK(f.m().quest(i).questId==i+1 && f.m().quest(i).rewardXP==10);
    CHECK(!std::strcmp(f.m().quest(i).title,titles[i]) && std::strlen(titles[i])<=QUEST_TITLE_MAX);
    CHECK(!questCompleted(f.m().quest(i)) && !f.m().rewarded(i));
  }
  CHECK(f.m().quest(4).kind==QuestKind::FocusSession && f.m().quest(0).kind==QuestKind::Manual);
  // Missing record: one initial write pins the Focus anchor; no XP.
  CHECK(f.missionStore.writes==1 && f.missionStore.slot.focusAnchorId==0 && f.p().totalXP()==0);
  f.reboot(); CHECK(f.m().loadResult()==MissionLoadResult::Loaded && f.missionStore.writes==1);
}
void completeOneAndProgressSteps() {  // T2, T3, T4
  Fixture f;
  CHECK(f.m().completeQuest(1)==QuestResult::Completed);
  CHECK(questCompleted(f.m().quest(0)) && f.m().rewarded(0));
  CHECK(f.p().totalXP()==10 && f.p().bonusXP()==10 && f.p().focusXP()==0);
  CHECK(f.m().progressPermille()==200 && f.m().completedCount()==1);
  char key[32]; ProgressService::formatRewardKey(key,sizeof(key),f.m().lastRewardKey());
  CHECK(!std::strcmp(key,"mission:1:1") && f.m().lastReward()==RewardResult::Granted);
  CHECK(f.m().completeQuest(2)==QuestResult::Completed && f.m().progressPermille()==400);
  CHECK(f.m().completeQuest(3)==QuestResult::Completed && f.m().progressPermille()==600);
  CHECK(f.m().completeQuest(4)==QuestResult::Completed && f.m().progressPermille()==800);
  CHECK(f.p().totalXP()==40);
  f.runFocus();  // The fifth (Focus-linked) quest completes from a real Focus session.
  CHECK(f.m().progressPermille()==1000 && f.m().completedCount()==5);
  CHECK(f.p().totalXP()==25+50 && f.p().focusXP()==25 && f.p().bonusXP()==50);
}
void repeatedTapNoDuplicate() {  // T5
  Fixture f;
  CHECK(f.m().completeQuest(2)==QuestResult::Completed);
  const unsigned attempts=f.missionStore.attempts;
  for (int i=0;i<200;++i) CHECK(f.m().completeQuest(2)==QuestResult::AlreadyCompleted);
  for (int i=0;i<200;++i) f.boot->update();
  CHECK(f.p().totalXP()==10 && f.missionStore.attempts==attempts);  // No write either.
  CHECK(f.m().completeQuest(99)==QuestResult::NotFound && f.m().completeQuest(0)==QuestResult::NotFound);
  CHECK(f.m().completeQuest(5)==QuestResult::FocusLinked && f.p().totalXP()==10);
}
void rebootNoDuplicate() {  // T6
  Fixture f;
  CHECK(f.m().completeQuest(1)==QuestResult::Completed && f.m().completeQuest(3)==QuestResult::Completed);
  for (int i=0;i<5;++i) {
    f.reboot();
    CHECK(f.p().totalXP()==20 && f.m().completedCount()==2 && f.m().rewarded(0) && f.m().rewarded(2));
    CHECK(f.m().completeQuest(1)==QuestResult::AlreadyCompleted);
    for (int j=0;j<50;++j) f.boot->update();
  }
  CHECK(f.p().totalXP()==20 && f.missionStore.slot.ledger.grantedCount==2);
}
void storageFailures() {  // T7
  Fixture f; f.missionStore.fail=true;
  CHECK(f.m().completeQuest(1)==QuestResult::SavePending);
  CHECK(f.m().savePending() && questCompleted(f.m().quest(0)));   // Shown as pending...
  CHECK(f.p().totalXP()==0 && f.m().error()==MissionError::StorageWrite);  // ...but no XP yet.
  CHECK(f.m().completeQuest(2)==QuestResult::SavePending && f.m().startNewDay(1)==DayResult::SavePending);
  const unsigned attempts=f.missionStore.attempts;
  f.clock.time+=MISSION_RETRY_MS-1; f.boot->update(); CHECK(f.missionStore.attempts==attempts);
  f.missionStore.fail=false; f.clock.time+=1; f.boot->update();
  CHECK(!f.m().savePending() && f.p().totalXP()==10 && f.m().error()==MissionError::None);
  CHECK(f.m().completedCount()==1);
  for (int i=0;i<100;++i) f.boot->update();
  CHECK(f.missionStore.attempts==attempts+1 && f.p().totalXP()==10);
  // Read I/O error: everything that writes is blocked, nothing is written.
  FakeClock clock; FakeFocusStorage fs; FakeMissionStorage ms; ms.readError=true;
  Boot b(clock,fs,ms);
  CHECK(b.missions.storageReadBlocked() && b.missions.error()==MissionError::StorageRead);
  CHECK(b.missions.completeQuest(1)==QuestResult::Blocked && b.missions.startNewDay(1)==DayResult::Blocked);
  CHECK(!b.missions.setTitle(1,"NEW TITLE"));
  b.focus.start(); clock.time+=FOCUS_DURATION_MS; b.update();
  CHECK(ms.attempts==0 && b.progress.totalXP()==25);  // Focus +25 unaffected.
}
void corruptedRecords() {  // T8
  MissionRecord variants[12];
  for (auto& r:variants) r=defaultMissionRecord();
  variants[0].checksum^=1;
  variants[1].version=2; sealMissionRecord(variants[1]);
  variants[2].bytes=167; sealMissionRecord(variants[2]);
  variants[3].dayId=0; variants[3].ledger.openDayId=0; sealMissionRecord(variants[3]);
  variants[4].quests[1].questId=1; sealMissionRecord(variants[4]);           // Duplicate ID.
  variants[5].ledger.rewardedMask=1; variants[5].ledger.grantedCount=1;
  variants[5].ledger.bonusXP=10; sealMissionRecord(variants[5]);            // Reward w/o completion.
  variants[6].quests[0].title[0]='a'; sealMissionRecord(variants[6]);       // Not normalized.
  variants[7].quests[0].title[15]='X'; sealMissionRecord(variants[7]);      // Padding not zero.
  variants[8].ledger.bonusXP=5000; sealMissionRecord(variants[8]);          // XP without grants.
  variants[9].ledger.openDayId=2; sealMissionRecord(variants[9]);           // Ledger/day mismatch.
  variants[10].reserved[1]=7; sealMissionRecord(variants[10]);
  variants[11].quests[2].kind=static_cast<QuestKind>(9); sealMissionRecord(variants[11]);
  for (const auto& r:variants) {
    CHECK(!validMissionRecord(r));
    FakeClock clock; FakeFocusStorage fs; FakeMissionStorage ms; ms.present=true; ms.slot=r;
    Boot b(clock,fs,ms);
    CHECK(b.missions.loadResult()==MissionLoadResult::Invalid && b.missions.error()==MissionError::InvalidRestore);
    CHECK(b.missions.dayId()==1 && b.missions.completedCount()==0 && b.progress.totalXP()==0);
    CHECK(ms.attempts==0);  // Bad blob kept for investigation until a user action.
    CHECK(b.missions.completeQuest(1)==QuestResult::Completed && b.progress.totalXP()==10);
  }
  // A/B slot selection: newest good wins; a corrupt newest falls back one commit.
  SlotRead slots[2];
  slots[0].status=SlotStatus::Valid; slots[0].record=defaultMissionRecord();
  slots[0].record.generation=7; sealMissionRecord(slots[0].record);
  slots[1]=slots[0]; slots[1].record.generation=8; sealMissionRecord(slots[1].record);
  MissionRecord out; uint8_t index=9;
  CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::Loaded && index==1 && out.generation==8);
  slots[1].record.checksum^=1;  // Status says valid but bytes are not: still rejected.
  CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::Recovered && index==0 && out.generation==7);
  slots[1].status=SlotStatus::Invalid;
  CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::Recovered && index==0);
  slots[0].status=SlotStatus::Missing; CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::Invalid);
  slots[1].status=SlotStatus::Missing; CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::Missing);
  slots[0].status=SlotStatus::Valid; slots[1].status=SlotStatus::IoError;
  CHECK(selectMissionSlot(slots,out,index)==MissionLoadResult::IoError);  // Never guess past I/O.
}
void newDayKeepsXp() {  // T9, T10
  Fixture f;
  CHECK(f.m().completeQuest(1)==QuestResult::Completed && f.m().completeQuest(2)==QuestResult::Completed);
  f.runFocus();
  CHECK(f.p().totalXP()==25+30 && f.m().completedCount()==3);
  const FocusRecord focusBefore=f.focusStore.slot;
  CHECK(f.m().startNewDay(2)==DayResult::Stale && f.m().dayId()==1);  // Wrong expected day.
  CHECK(f.m().startNewDay(1)==DayResult::Started && f.m().dayId()==2);
  CHECK(f.m().startNewDay(1)==DayResult::Stale && f.m().dayId()==2);  // Same confirmation twice.
  CHECK(f.m().completedCount()==0 && f.m().progressPermille()==0 && !f.m().rewarded(0));
  CHECK(f.p().totalXP()==55 && f.p().focusXP()==25 && f.p().completedFocusSessions()==1);
  CHECK(!std::memcmp(&focusBefore,&f.focusStore.slot,sizeof(focusBefore)));  // Focus untouched.
  CHECK(!std::strcmp(f.m().quest(1).title,"PRACTICE PYTHON"));
  CHECK(f.m().completeQuest(1)==QuestResult::Completed && f.p().totalXP()==65);  // mission:2:1
  CHECK(ProgressService::rewardGranted(f.m().committed().ledger,RewardKey{1,1}));  // Day 1 closed.
  RewardLedger ledger=f.m().committed().ledger;
  CHECK(ProgressService::grantRewardOnce(ledger,RewardKey{1,2},10)==RewardResult::DayClosed);
  CHECK(ProgressService::grantRewardOnce(ledger,RewardKey{2,1},10)==RewardResult::AlreadyRewarded);
  CHECK(ProgressService::grantRewardOnce(ledger,RewardKey{3,1},10)==RewardResult::Invalid);
  CHECK(!ProgressService::openRewardDay(ledger,2) && ProgressService::openRewardDay(ledger,3));
  f.reboot(); CHECK(f.m().dayId()==2 && f.p().totalXP()==65 && f.m().completedCount()==1);
  // Focus completed on day 1 does not complete day 2's Focus quest by itself.
  for (int i=0;i<10;++i) f.boot->update();
  CHECK(!questCompleted(f.m().quest(4)) && f.p().totalXP()==65);
}
void focusCompletedRewardsOnce() {  // T11
  Fixture f;
  f.boot->focus.start(); f.clock.time+=60000; f.boot->update();
  CHECK(f.boot->focus.reset());  // Opening/aborting Focus never completes the quest.
  for (int i=0;i<10;++i) f.boot->update();
  CHECK(!questCompleted(f.m().quest(4)) && f.p().totalXP()==0);
  f.boot->focus.start(); f.clock.time+=FOCUS_DURATION_MS-1; f.boot->update();
  CHECK(!questCompleted(f.m().quest(4)));
  f.clock.time+=1; f.boot->update();
  CHECK(questCompleted(f.m().quest(4)) && f.m().rewarded(4));
  CHECK(f.p().focusXP()==25 && f.p().bonusXP()==10 && f.p().totalXP()==35);
  const unsigned writes=f.missionStore.writes;
  for (int i=0;i<1000;++i) { f.clock.time+=10; f.boot->update(); }
  CHECK(f.p().totalXP()==35 && f.missionStore.writes==writes);
  CHECK(f.boot->focus.start() && f.boot->focus.mode()==FocusMode::Break);  // 5-minute break.
  f.clock.time+=BREAK_DURATION_MS; f.boot->update();
  CHECK(f.boot->focus.mode()==FocusMode::Focus && f.p().totalXP()==35);
  f.runFocus();  // A second Focus the same day: +25 only.
  CHECK(f.p().focusXP()==50 && f.p().bonusXP()==10 && f.p().totalXP()==60);
  f.reboot(); for (int i=0;i<10;++i) f.boot->update();
  CHECK(f.p().totalXP()==60 && f.missionStore.writes==writes);
}
void rebootBetweenSaves() {  // T12
  // (a) Power lost before the mission commit: neither completion nor XP exists.
  {
    Fixture f; f.missionStore.fail=true;
    CHECK(f.m().completeQuest(1)==QuestResult::SavePending && f.p().totalXP()==0);
    f.missionStore.fail=false; f.reboot();
    CHECK(!questCompleted(f.m().quest(0)) && f.p().totalXP()==0);
    CHECK(f.m().completeQuest(1)==QuestResult::Completed && f.p().totalXP()==10);
  }
  // (b) Commit landed but was reported failed: reboot sees completion+XP once.
  {
    Fixture f; f.missionStore.fail=f.missionStore.commitThenFail=true;
    CHECK(f.m().completeQuest(1)==QuestResult::SavePending && f.p().totalXP()==0);
    const MissionRecord landed=f.missionStore.slot;
    f.missionStore.fail=false; f.clock.time+=MISSION_RETRY_MS; f.boot->update();
    CHECK(!std::memcmp(&landed,&f.missionStore.slot,sizeof(landed)));  // Same bytes re-written.
    CHECK(f.p().totalXP()==10);
    f.reboot(); CHECK(f.p().totalXP()==10 && f.m().completeQuest(1)==QuestResult::AlreadyCompleted);
  }
  // (c) Focus +25 committed, reboot before the mission +10 commit: recovered once.
  {
    Fixture f; f.missionStore.fail=true;
    f.runFocus();
    CHECK(f.p().focusXP()==25 && f.p().bonusXP()==0 && f.m().savePending());
    f.missionStore.fail=false; f.reboot();
    CHECK(f.p().totalXP()==25 && !questCompleted(f.m().quest(4)));
    f.boot->update();
    CHECK(questCompleted(f.m().quest(4)) && f.p().totalXP()==35);
    f.reboot(); for (int i=0;i<10;++i) f.boot->update();
    CHECK(f.p().totalXP()==35 && f.missionStore.slot.ledger.grantedCount==1);
  }
  // (d) Day change written but reported failed: reboot lands on day 2 exactly once.
  {
    Fixture f; CHECK(f.m().completeQuest(1)==QuestResult::Completed);
    f.missionStore.fail=f.missionStore.commitThenFail=true;
    CHECK(f.m().startNewDay(1)==DayResult::SavePending && f.m().dayId()==2);
    f.missionStore.fail=false; f.reboot();
    CHECK(f.m().dayId()==2 && f.p().totalXP()==10 && f.m().startNewDay(1)==DayResult::Stale);
  }
}
void focusTimerUnaffected() {  // T14 (service level)
  Fixture f; CHECK(f.boot->focus.start());
  f.clock.time+=30000; f.boot->update();
  CHECK(f.m().completeQuest(1)==QuestResult::Completed && f.m().startNewDay(1)==DayResult::Started);
  CHECK(f.m().setTitle(2,"learn sql joins"));
  CHECK(f.boot->focus.state()==FocusState::Running && f.boot->focus.remainingMs()==FOCUS_DURATION_MS-30000);
  CHECK(f.focusStore.attempts==1);  // Mission actions never write the Focus record.
  f.clock.time+=FOCUS_DURATION_MS-30000; f.boot->update();
  CHECK(f.p().focusXP()==25 && f.boot->focus.mode()==FocusMode::Break);
  CHECK(questCompleted(f.m().quest(4)) && f.p().totalXP()==45);
}
void titlesAndLimits() {
  Fixture f;
  CHECK(f.m().completeQuest(3)==QuestResult::Completed);
  CHECK(f.m().setTitle(3,"read 20 pages"));
  CHECK(!std::strcmp(f.m().quest(2).title,"READ 20 PAGES") && questCompleted(f.m().quest(2)));
  CHECK(f.m().completeQuest(3)==QuestResult::AlreadyCompleted && f.p().totalXP()==10);  // Rename != new key.
  CHECK(!f.m().setTitle(3,"") && !f.m().setTitle(3," LEADING") && !f.m().setTitle(3,"TRAILING "));
  CHECK(!f.m().setTitle(3,"SEVENTEEN CHARS!!") && !f.m().setTitle(3,"BAD_CHAR") && !f.m().setTitle(9,"OK"));
  CHECK(f.m().setTitle(3,"EXACTLY 16 CHARS"));
  f.reboot(); CHECK(!std::strcmp(f.m().quest(2).title,"EXACTLY 16 CHARS"));
  RewardLedger l=ProgressService::emptyLedger(1);
  CHECK(ProgressService::grantRewardOnce(l,RewardKey{1,0},10)==RewardResult::Invalid);
  CHECK(ProgressService::grantRewardOnce(l,RewardKey{1,33},10)==RewardResult::Invalid);
  CHECK(ProgressService::grantRewardOnce(l,RewardKey{1,1},0)==RewardResult::Invalid);
  CHECK(ProgressService::grantRewardOnce(l,RewardKey{1,1},101)==RewardResult::Invalid);
  l.bonusXP=UINT32_MAX-5; l.grantedCount=UINT32_MAX/100+1;
  CHECK(ProgressService::grantRewardOnce(l,RewardKey{1,1},10)==RewardResult::LimitReached);
  CHECK(ProgressService::validLedger(ProgressService::emptyLedger(1)) && !ProgressService::validLedger(RewardLedger{0,0,0,0}));
}
void calendarHookForM5() {
  FakeClock clock; FakeFocusStorage fs; FakeMissionStorage ms; FakeDays days;
  {
    Boot b(clock,fs,ms,&days);
    b.update(); CHECK(b.missions.dayId()==1 && ms.writes==1);  // No calendar: manual only.
    days.day=20000; b.update();
    CHECK(b.missions.dayId()==1 && ms.slot.calendarDay==20000);  // First valid clock: stamp.
    CHECK(b.missions.completeQuest(1)==QuestResult::Completed);
    days.day=19999; b.update(); CHECK(b.missions.dayId()==1);   // Backwards ignored.
    days.day=20001; b.update(); b.update();
    CHECK(b.missions.dayId()==2 && ms.slot.calendarDay==20001 && b.progress.totalXP()==10);
  }
  Boot again(clock,fs,ms,&days); again.update(); CHECK(again.missions.dayId()==2);
}
}  // namespace

int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"M01 load 5 default quests, one initial write",defaultsLoaded},
    {"M02-M04 complete, progress 20..100%, +10 XP",completeOneAndProgressSteps},
    {"M05 repeated TAP/update: no duplicate XP or write",repeatedTapNoDuplicate},
    {"M06 reboot: no duplicate reward",rebootNoDuplicate},
    {"M07 NVS write failure/pending retry and read I/O block",storageFailures},
    {"M08 corrupted/version/shape records and A/B fallback",corruptedRecords},
    {"M09-M10 NEW DAY resets completion, keeps XP, new keys",newDayKeepsXp},
    {"M11 FocusCompleted: +25 Focus and +10 mission once",focusCompletedRewardsOnce},
    {"M12 reboot between completion and reward commits",rebootBetweenSaves},
    {"M14 mission actions never touch the Focus timer",focusTimerUnaffected},
    {"M15 Serial title update and reward guards",titlesAndLimits},
    {"M16 calendar day interface for M5 TimeService",calendarHookForM5}
  };
  for (const auto& test:tests) {
    const int before=failures; test.run();
    std::printf("%s: %s\n",failures==before?"PASS":"FAIL",test.name);
  }
  std::printf("RESULT: %u mission tests, %d failures\n",
              static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
