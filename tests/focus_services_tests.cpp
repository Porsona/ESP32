#include <cstdio>
#include <cstring>
#include <climits>
#include "services/FocusSessionService.h"

using namespace deskos;
namespace {
int failures=0;
#define CHECK(expression) do { if (!(expression)) { std::printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expression); ++failures; return; } } while (0)

struct FakeClock : IFocusClock {
  uint32_t time=0;
  uint32_t nowMs() const override { return time; }
  void advance(uint32_t ms) { time+=ms; }
};
struct FakeStorage : IFocusStorage {
  FocusRecord slot{};
  bool present=false,fail=false,commitThenFail=false,readError=false;
  unsigned attempts=0;
  FocusLoadResult load(FocusRecord& record) override {
    if (readError) return FocusLoadResult::IoError;
    if (!present) return FocusLoadResult::Missing;
    record=slot; return FocusLoadResult::Loaded; // Service must validate too.
  }
  bool save(const FocusRecord& record) override {
    ++attempts;
    if (!validFocusRecord(record)) return false;
    if (fail && !commitThenFail) return false;
    slot=record; present=true;
    return !fail;
  }
};
struct Fixture {
  FakeClock clock;
  FakeStorage storage;
  ProgressService progress;
  FocusSessionService timer;
  Fixture() : timer(clock,storage,progress) { timer.begin(); }
};

void initialAndTransitions() {
  Fixture f;
  CHECK(f.timer.state()==FocusState::Idle);
  CHECK(f.timer.remainingMs()==1500000 && f.timer.mode()==FocusMode::Focus);
  CHECK(f.progress.totalXP()==0 && f.storage.attempts==0);
  CHECK(!f.timer.pause() && !f.timer.resume());
  CHECK(f.timer.start() && !f.timer.start());
  CHECK(f.progress.data().currentSessionId==1);
  f.clock.advance(10001);
  CHECK(f.timer.remainingMs()==1489999 && f.timer.remainingSeconds()==1490);
  CHECK(f.timer.pause());
  const uint32_t paused=f.timer.remainingMs();
  f.clock.advance(100000); f.timer.update();
  CHECK(f.timer.remainingMs()==paused && f.progress.totalXP()==0);
  CHECK(f.timer.resume() && !f.timer.resume());
  f.clock.advance(paused); f.timer.update();
  CHECK(f.timer.state()==FocusState::Completed && f.timer.mode()==FocusMode::Break);
  CHECK(f.timer.remainingMs()==300000 && f.progress.totalXP()==25);
  CHECK(f.progress.completedFocusSessions()==1);
}
void preciseDeadlineAndDeduplication() {
  Fixture f; CHECK(f.timer.start());
  f.clock.advance(1499999); f.timer.update();
  CHECK(f.timer.state()==FocusState::Running && f.timer.remainingMs()==1);
  CHECK(f.progress.totalXP()==0);
  f.clock.advance(1); f.timer.update();
  const unsigned saved=f.storage.attempts;
  for (unsigned i=0;i<10000;++i) { f.clock.advance(1); f.timer.update(); }
  CHECK(f.storage.attempts==saved && f.progress.totalXP()==25);
  CHECK(f.progress.data().lastRewardedSessionId==1);
}
void breakAndSecondFocus() {
  Fixture f; CHECK(f.timer.start());
  f.clock.advance(FOCUS_DURATION_MS); f.timer.update();
  f.clock.advance(BREAK_DURATION_MS); f.timer.update();
  CHECK(f.timer.remainingMs()==BREAK_DURATION_MS); // Waits for START.
  CHECK(f.timer.start());
  CHECK(f.progress.data().currentSessionId==1);
  f.clock.advance(BREAK_DURATION_MS-1); f.timer.update();
  CHECK(f.timer.mode()==FocusMode::Break && f.progress.totalXP()==25);
  f.clock.advance(1); f.timer.update();
  CHECK(f.timer.mode()==FocusMode::Focus && f.timer.state()==FocusState::Completed);
  CHECK(f.progress.totalXP()==25 && f.progress.completedFocusSessions()==1);
  CHECK(f.timer.start() && f.progress.data().currentSessionId==2);
  f.clock.advance(FOCUS_DURATION_MS); f.timer.update();
  CHECK(f.progress.totalXP()==50 && f.progress.completedFocusSessions()==2);
}
void resetAbortsWithoutReward() {
  Fixture f; CHECK(f.timer.start());
  f.clock.advance(300000); CHECK(f.timer.reset());
  CHECK(f.timer.state()==FocusState::Idle && f.timer.remainingMs()==FOCUS_DURATION_MS);
  CHECK(f.progress.totalXP()==0 && f.progress.data().currentSessionId==1);
  CHECK(f.timer.start() && f.progress.data().currentSessionId==2);
  CHECK(f.timer.pause() && f.timer.reset());
  CHECK(f.progress.totalXP()==0 && f.timer.state()==FocusState::Idle);
  CHECK(f.timer.start()); f.clock.advance(FOCUS_DURATION_MS); f.timer.update();
  CHECK(f.timer.start() && f.timer.mode()==FocusMode::Break);
  CHECK(f.timer.reset());
  CHECK(f.timer.mode()==FocusMode::Focus && f.progress.totalXP()==25);
}
void rollover() {
  Fixture f; f.clock.time=UINT32_MAX-500; CHECK(f.timer.start());
  f.clock.advance(1000); f.timer.update();
  CHECK(f.timer.remainingMs()==FOCUS_DURATION_MS-1000);
  CHECK(f.timer.pause()); f.clock.advance(9000); CHECK(f.timer.resume());
  f.clock.advance(FOCUS_DURATION_MS-1000); f.timer.update();
  CHECK(f.progress.totalXP()==25 && f.timer.state()==FocusState::Completed);
}
void checkpointRate() {
  Fixture f; CHECK(f.timer.start()); CHECK(f.storage.attempts==1);
  f.clock.advance(59999); f.timer.update(); CHECK(f.storage.attempts==1);
  f.clock.advance(1); f.timer.update(); CHECK(f.storage.attempts==2);
  CHECK(f.storage.slot.remainingMs==FOCUS_DURATION_MS-60000);
  for (int i=0;i<1000;++i) f.timer.update();
  CHECK(f.storage.attempts==2);
  f.clock.advance(60000); f.timer.update(); CHECK(f.storage.attempts==3);
  CHECK(f.timer.pause()); const unsigned saved=f.storage.attempts;
  f.clock.advance(600000); f.timer.update(); CHECK(f.storage.attempts==saved);
}
void offscreenCoreUpdates() {
  Fixture f; CHECK(f.timer.start());
  // Simulate Home/other/recovery views: only core service updates, no screen object.
  for (unsigned minute=0;minute<25;++minute) {
    f.clock.advance(60000); f.timer.update();
  }
  CHECK(f.timer.mode()==FocusMode::Break && f.progress.totalXP()==25);
  CHECK(f.progress.completedFocusSessions()==1);
}
void restoreCheckpointPaused() {
  Fixture f; CHECK(f.timer.start());
  f.clock.advance(60000); f.timer.update(); f.clock.advance(30000);
  FakeClock rebootClock; ProgressService rebootProgress;
  FocusSessionService reboot(rebootClock,f.storage,rebootProgress); reboot.begin();
  CHECK(reboot.restoredRunning() && reboot.state()==FocusState::Paused);
  CHECK(reboot.remainingMs()==1440000 && rebootProgress.data().currentSessionId==1);
  const unsigned saved=f.storage.attempts;
  rebootClock.advance(3600000); reboot.update();
  CHECK(reboot.remainingMs()==1440000 && f.storage.attempts==saved);
  CHECK(reboot.resume()); rebootClock.advance(1440000); reboot.update();
  CHECK(rebootProgress.totalXP()==25 && rebootProgress.completedFocusSessions()==1);
  FakeClock againClock; ProgressService againProgress;
  FocusSessionService again(againClock,f.storage,againProgress); again.begin();
  CHECK(!again.restoredRunning() && again.state()==FocusState::Completed);
  for (int i=0;i<100;++i) again.update();
  CHECK(againProgress.totalXP()==25 && again.mode()==FocusMode::Break);
}
void invalidRestore() {
  FocusRecord variants[7];
  for (auto& record:variants) record=defaultFocusRecord();
  variants[0].checksum^=1;
  variants[1].version=99; sealFocusRecord(variants[1]);
  variants[2].bytes=39; sealFocusRecord(variants[2]);
  variants[3].state=static_cast<FocusState>(99); sealFocusRecord(variants[3]);
  variants[4].remainingMs=FOCUS_DURATION_MS+1; sealFocusRecord(variants[4]);
  variants[5].progress.lastRewardedSessionId=1; sealFocusRecord(variants[5]);
  variants[6].reserved[0]=1; sealFocusRecord(variants[6]);
  for (const auto& record:variants) {
    FakeClock clock; FakeStorage storage; ProgressService progress;
    storage.present=true; storage.slot=record;
    FocusSessionService timer(clock,storage,progress); timer.begin();
    CHECK(timer.loadResult()==FocusLoadResult::Invalid);
    CHECK(timer.error()==FocusError::InvalidRestore && timer.state()==FocusState::Idle);
    CHECK(timer.remainingMs()==FOCUS_DURATION_MS && progress.totalXP()==0);
    CHECK(storage.attempts==0); // Bad record retained for investigation until explicit action.
  }
  FakeClock clock; FakeStorage storage; ProgressService progress; storage.readError=true;
  FocusSessionService timer(clock,storage,progress); timer.begin();
  CHECK(timer.error()==FocusError::StorageRead && timer.state()==FocusState::Idle);
  CHECK(timer.storageReadBlocked() && !timer.start() && !timer.reset());
  CHECK(storage.attempts==0); // An I/O failure must not replace unread progress with zeros.
}
void actionWriteFailures() {
  Fixture f; f.storage.fail=true;
  CHECK(!f.timer.start() && f.timer.state()==FocusState::Idle);
  CHECK(f.progress.data().currentSessionId==0 && f.timer.error()==FocusError::StorageWrite);
  f.storage.fail=false; CHECK(f.timer.start()); f.clock.advance(10000);
  f.storage.fail=true; CHECK(!f.timer.pause());
  CHECK(f.timer.state()==FocusState::Running && f.timer.remainingMs()==1490000);
  CHECK(!f.timer.reset() && f.timer.state()==FocusState::Running);
  f.clock.advance(1000); CHECK(f.timer.remainingMs()==1489000);
  f.storage.fail=false; CHECK(f.timer.pause());
  f.storage.fail=true; CHECK(!f.timer.resume() && f.timer.state()==FocusState::Paused);
  CHECK(f.progress.totalXP()==0);
}
void failedCheckpointThrottled() {
  Fixture f; CHECK(f.timer.start()); f.storage.fail=true;
  f.clock.advance(60000); f.timer.update(); CHECK(f.storage.attempts==2);
  f.clock.advance(59999); f.timer.update(); CHECK(f.storage.attempts==2);
  CHECK(f.timer.remainingMs()==FOCUS_DURATION_MS-119999);
  f.clock.advance(1); f.timer.update(); CHECK(f.storage.attempts==3);
  CHECK(f.storage.slot.remainingMs==FOCUS_DURATION_MS);
  f.storage.fail=false; f.clock.advance(60000); f.timer.update();
  CHECK(f.storage.attempts==4 && f.storage.slot.remainingMs==FOCUS_DURATION_MS-180000);
}
void completionWriteFailureAndRetry() {
  Fixture f; CHECK(f.timer.start()); f.storage.fail=true;
  f.clock.advance(FOCUS_DURATION_MS); f.timer.update();
  CHECK(f.timer.savePending() && f.timer.state()==FocusState::Completed);
  CHECK(f.progress.totalXP()==0 && !f.timer.start() && !f.timer.reset());
  const unsigned saved=f.storage.attempts;
  f.clock.advance(4999); f.timer.update(); CHECK(f.storage.attempts==saved);
  f.storage.fail=false; f.clock.advance(1); f.timer.update();
  CHECK(!f.timer.savePending() && f.progress.totalXP()==25);
  CHECK(f.progress.completedFocusSessions()==1);
  for (int i=0;i<1000;++i) f.timer.update();
  CHECK(f.storage.attempts==saved+1 && f.progress.totalXP()==25);
}
void ambiguousCompletionAndReboot() {
  Fixture f; CHECK(f.timer.start()); f.storage.fail=f.storage.commitThenFail=true;
  f.clock.advance(FOCUS_DURATION_MS); f.timer.update();
  CHECK(f.timer.savePending() && f.progress.totalXP()==0);
  const FocusRecord committed=f.storage.slot;
  FakeClock rebootClock; ProgressService rebootProgress;
  FocusSessionService reboot(rebootClock,f.storage,rebootProgress); reboot.begin();
  CHECK(rebootProgress.totalXP()==25 && reboot.state()==FocusState::Completed);
  reboot.update(); CHECK(rebootProgress.totalXP()==25);
  f.storage.fail=false; f.clock.advance(COMPLETION_RETRY_MS); f.timer.update();
  CHECK(std::memcmp(&committed,&f.storage.slot,sizeof(committed))==0);
  CHECK(f.progress.totalXP()==25);
}
void failedCompletionPowerLoss() {
  Fixture f; CHECK(f.timer.start());
  f.clock.advance(FOCUS_DURATION_MS-60000); f.timer.update();
  f.storage.fail=true; f.clock.advance(60000); f.timer.update();
  CHECK(f.timer.savePending() && f.progress.totalXP()==0);
  FakeClock rebootClock; ProgressService rebootProgress;
  FocusSessionService reboot(rebootClock,f.storage,rebootProgress); reboot.begin();
  CHECK(reboot.state()==FocusState::Paused && reboot.remainingMs()==60000);
  f.storage.fail=false; CHECK(reboot.resume()); rebootClock.advance(60000); reboot.update();
  CHECK(rebootProgress.totalXP()==25 && rebootProgress.completedFocusSessions()==1);
}
void progressGuards() {
  ProgressData data{}; CHECK(ProgressService::beginFocus(data));
  CHECK(ProgressService::rewardFocus(data,2)==RewardResult::Invalid);
  CHECK(ProgressService::rewardFocus(data,1)==RewardResult::Granted);
  CHECK(ProgressService::rewardFocus(data,1)==RewardResult::AlreadyRewarded);
  CHECK(data.totalXP==25 && data.completedFocusSessions==1);
  CHECK(ProgressService::beginFocus(data));
  CHECK(ProgressService::rewardFocus(data,0)==RewardResult::Invalid);
  CHECK(ProgressService::rewardFocus(data,1)==RewardResult::AlreadyRewarded);
  CHECK(ProgressService::rewardFocus(data,2)==RewardResult::Granted && data.totalXP==50);
  data.currentSessionId=UINT32_MAX; CHECK(!ProgressService::beginFocus(data));
  data.totalXP=UINT32_MAX; CHECK(ProgressService::rewardFocus(data,UINT32_MAX)==RewardResult::LimitReached);
}
void completionWinsAtDeadline() {
  Fixture f; CHECK(f.timer.start()); f.clock.advance(FOCUS_DURATION_MS);
  CHECK(!f.timer.pause()); CHECK(f.progress.totalXP()==25);
  CHECK(f.timer.reset() && f.progress.totalXP()==25);
  CHECK(f.timer.remainingMs()==FOCUS_DURATION_MS);
}
} // namespace

int main() {
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"initial/start/pause/resume",initialAndTransitions},
    {"exact 25-minute deadline and duplicate updates",preciseDeadlineAndDeduplication},
    {"5-minute break, manual START and second focus",breakAndSecondFocus},
    {"reset aborts without reward, preserves progress",resetAbortsWithoutReward},
    {"uint32 millis rollover",rollover},
    {"checkpoint maximum once per 60 seconds",checkpointRate},
    {"core updates while screen is absent",offscreenCoreUpdates},
    {"running checkpoint restores PAUSED, completed restores once",restoreCheckpointPaused},
    {"missing/corrupt/version/shape and read errors",invalidRestore},
    {"start/pause/resume/reset write failures",actionWriteFailures},
    {"failed checkpoint throttled, RAM time continues",failedCheckpointThrottled},
    {"completion write failure and cached retry",completionWriteFailureAndRetry},
    {"ambiguous completion commit and reboot",ambiguousCompletionAndReboot},
    {"power loss before completion commit",failedCompletionPowerLoss},
    {"progress deduplication, IDs and overflow",progressGuards},
    {"completion takes precedence at deadline",completionWinsAtDeadline}
  };
  for (const auto& test:tests) {
    const int before=failures; test.run();
    std::printf("%s: %s\n",failures==before?"PASS":"FAIL",test.name);
  }
  std::printf("RESULT: %u tests, %d failures (production durations 1500000/300000 ms)\n",
              static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
