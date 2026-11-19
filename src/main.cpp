#include <Arduino.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "DisplayHAL.h"
#include "DiagnosticScreen.h"
#include "hal/TouchHAL.h"
#include "core/TouchCalibrationStore.h"
#include "apps/TouchCalibrationScreen.h"
#include "apps/TouchTestScreen.h"
#include "core/AppManager.h"
#include "ui/PixelRenderer.h"
#include "apps/HomeScreen.h"
#include "apps/PlaceholderScreen.h"
#include "hal/FocusClock.h"
#include "storage/FocusStorageNVS.h"
#include "apps/FocusQuestScreen.h"
#include "storage/MissionStorageNVS.h"
#include "services/FocusCompletionAdapter.h"
#include "apps/DailyMissionsScreen.h"

DisplayHAL display;
TouchHAL touch;
constexpr char FIRMWARE_VERSION[] = "M2_MISSIONS_01";
DiagnosticScreen diagnostics(display, FIRMWARE_VERSION);
TouchCalibrationScreen calibrationScreen(display);
TouchTestScreen touchTest(display);
touchcal::CalibrationRecord calibration{};
deskos::AppManager appManager;
deskos::PixelRenderer pixelRenderer(display);
deskos::HomeScreen homeScreen(pixelRenderer,appManager);
deskos::PlaceholderScreen placeholderScreen(pixelRenderer,appManager);
deskos::FocusClock focusClock;
deskos::FocusStorageNVS focusStorage;
deskos::ProgressService progressService;
deskos::FocusSessionService focusService(focusClock,focusStorage,progressService);
deskos::FocusQuestScreen focusScreen(pixelRenderer,appManager,focusService,progressService);
deskos::MissionStorageNVS missionStorage;
deskos::FocusCompletionAdapter focusCompletions(focusService,progressService);
// No IDaySource in M2: quest days advance only via NEW DAY (TimeService/NTP is M5).
deskos::DailyMissionService missionService(focusClock,missionStorage,progressService,focusCompletions);
deskos::DailyMissionsScreen missionsScreen(pixelRenderer,appManager,missionService,progressService);

enum class View { Calibration, Test, Diagnostics, Shell };
View view=View::Calibration;
bool haveCalibration=false, diagnosticsWaitForRelease=true, observedRaw=false;
bool shellReady=false, shellWaitForRelease=true;
uint32_t lastTouchLog=0, lastHeapLog=0;
uint32_t lastShellUpdate=0;
uint32_t lastFocusRevision=0, lastMissionRevision=0, lastMissionGrants=0;

void logFocusState() {
  if (lastFocusRevision==focusService.revision()) return;
  lastFocusRevision=focusService.revision();
  const auto& progress=progressService.data();
  Serial.printf("FOCUS: ms=%lu mode=%s state=%s remaining_ms=%lu xp=%lu sessions=%lu id=%lu rewarded_id=%lu save_pending=%u error=%s\n",
    static_cast<unsigned long>(millis()),deskos::focusModeName(focusService.mode()),
    deskos::focusStateName(focusService.state()),static_cast<unsigned long>(focusService.remainingMs()),
    static_cast<unsigned long>(progress.totalXP),static_cast<unsigned long>(progress.completedFocusSessions),
    static_cast<unsigned long>(progress.currentSessionId),static_cast<unsigned long>(progress.lastRewardedSessionId),
    focusService.savePending()?1U:0U,deskos::focusErrorName(focusService.error()));
}

void logMissionState(bool quests=false) {
  const uint32_t grants=progressService.ledger().grantedCount;
  if (grants!=lastMissionGrants) {
    char key[32];
    deskos::ProgressService::formatRewardKey(key,sizeof(key),missionService.lastRewardKey());
    Serial.printf("MISSION_REWARD: key=%s result=%s bonus_xp=%lu total_xp=%lu\n",key,
                  deskos::ProgressService::resultName(missionService.lastReward()),
                  static_cast<unsigned long>(progressService.bonusXP()),
                  static_cast<unsigned long>(progressService.totalXP()));
    lastMissionGrants=grants;
  }
  if (!quests && lastMissionRevision==missionService.revision()) return;
  lastMissionRevision=missionService.revision();
  const auto& record=missionService.committed();
  Serial.printf("MISSIONS: ms=%lu day=%lu done=%u/%u focus_anchor=%ld rewards=%lu bonus_xp=%lu total_xp=%lu generation=%lu save_pending=%u error=%s\n",
    static_cast<unsigned long>(millis()),static_cast<unsigned long>(missionService.dayId()),
    static_cast<unsigned>(missionService.completedCount()),static_cast<unsigned>(missionService.questCount()),
    record.focusAnchorId==deskos::FOCUS_ANCHOR_UNKNOWN?-1L:static_cast<long>(record.focusAnchorId),
    static_cast<unsigned long>(progressService.ledger().grantedCount),
    static_cast<unsigned long>(progressService.bonusXP()),static_cast<unsigned long>(progressService.totalXP()),
    static_cast<unsigned long>(record.generation),missionService.savePending()?1U:0U,
    deskos::missionErrorName(missionService.error()));
  if (!quests) return;
  for (uint8_t i=0;i<missionService.questCount();++i) {
    const auto& q=missionService.quest(i);
    Serial.printf("QUEST: id=%u title=\"%s\" kind=%s xp=%u done=%u rewarded=%u\n",
                  static_cast<unsigned>(q.questId),q.title,deskos::questKindName(q.kind),
                  static_cast<unsigned>(q.rewardXP),deskos::questCompleted(q)?1U:0U,missionService.rewarded(i)?1U:0U);
  }
}

// "QTITLE <id> <TEXT>": rename a quest; questId (reward key) and status are kept.
void titleCommand(const char* args) {
  char* end=nullptr;
  const long id=std::strtol(args,&end,10);
  if (end==args || *end!=' ' || id<1 || id>255) { Serial.println("QTITLE: USAGE QTITLE <ID> <TITLE 1-16 CHARS A-Z 0-9 :+-/.!?#>"); return; }
  const bool ok=missionService.setTitle(static_cast<uint8_t>(id),end+1);
  Serial.printf("QTITLE: %s id=%ld\n",ok?(missionService.savePending()?"SAVE_PENDING":"SAVED"):"REJECTED",id);
  logMissionState(true);
}

const char* viewName() {
  return view==View::Calibration?"CAL":view==View::Test?"TEST":view==View::Shell?"SHELL":"DIAG";
}

void leaveShell() {
  if (view==View::Shell) appManager.suspend();
}

void startCalibration(uint32_t now) {
  leaveShell();
  view=View::Calibration;
  calibrationScreen.begin(now);
}

void showTest(uint32_t now) {
  if (!haveCalibration) { startCalibration(now); return; }
  leaveShell();
  view=View::Test;
  touchTest.begin();
}

void showDiagnostics(uint32_t now) {
  leaveShell();
  view=View::Diagnostics; diagnosticsWaitForRelease=true;
  diagnostics.show(now);
  Serial.println("RECOVERY: DIAG | tap/NEXT=next | CAL=recalibrate | TEST=touch test | HOME=shell");
}

void showShell(uint32_t now) {
  if (!haveCalibration) { startCalibration(now); return; }
  if (!shellReady) { showDiagnostics(now); return; }
  appManager.cancelTouch();
  view=View::Shell; shellWaitForRelease=true; lastShellUpdate=now;
  appManager.home();
}

void registerShell() {
  shellReady=appManager.registerApp(deskos::AppId::Home,homeScreen);
  for (uint8_t id=1;id<deskos::APP_COUNT;++id) {
    const auto appId=static_cast<deskos::AppId>(id);
    deskos::IApp& screen=appId==deskos::AppId::Focus?static_cast<deskos::IApp&>(focusScreen):
                         appId==deskos::AppId::Missions?static_cast<deskos::IApp&>(missionsScreen):
                                                        static_cast<deskos::IApp&>(placeholderScreen);
    if (!appManager.registerApp(appId,screen)) shellReady=false;
  }
  Serial.printf("SHELL_REGISTRY: %s ids=7 screen_instances=4\n",shellReady?"READY":"FAILED");
}

void dispatchShellTouch(const TouchFrame& frame) {
  if (!frame.updated) return;
  // Recovery/view changes consume the old finger before accepting a new press.
  if (shellWaitForRelease) {
    if (!frame.down && !frame.rawValid) shellWaitForRelease=false;
    return;
  }
  if (!frame.events && !(frame.down && frame.rawValid)) return;
  const TouchPixel pixel=touchcal::map(calibration,frame.raw);
  // This UI event type contains no raw coordinates or calibration logic.
  deskos::TouchEvent event{deskos::TouchEvent::Type::Move,pixel.x,pixel.y,millis(),frame.heldMs};
  if (frame.events&TOUCH_PRESS) { event.type=deskos::TouchEvent::Type::Press; appManager.onTouch(event); }
  else if (frame.down && frame.rawValid) appManager.onTouch(event);
  if (frame.events&TOUCH_RELEASE) { event.type=deskos::TouchEvent::Type::Release; appManager.onTouch(event); }
  if (frame.events&TOUCH_TAP) { event.type=deskos::TouchEvent::Type::Tap; appManager.onTouch(event); }
}

void printCalibration() {
  if (!haveCalibration) { Serial.println("TOUCH_CAL: NO_VALID_MAP"); return; }
  const auto* c=calibration.coefficient;
  Serial.printf("TOUCH_CAL: AFFINE x=%.7f*rawX%+.7f*rawY%+.3f y=%.7f*rawX%+.7f*rawY%+.3f\n",
                c[0],c[1],c[2],c[3],c[4],c[5]);
  Serial.printf("TOUCH_CAL: ERRORS rms=%.2f max=%.2f independent_max=%.2f\n",
                calibration.rmsError,calibration.maxError,calibration.verificationMaxError);
}

void printHelp() {
  Serial.println("COMMANDS: HOME | BACK | CAL | RETRY | TEST | DIAG | NEXT | STATUS | MISSIONS | QTITLE <ID> <TITLE> | HELP (newline)");
}

void command(const char* line, uint32_t now) {
  if (!std::strcmp(line,"CAL") || !std::strcmp(line,"RETRY")) startCalibration(now);
  else if (!std::strcmp(line,"DIAG")) showDiagnostics(now);
  else if (!std::strcmp(line,"TEST")) showTest(now);
  else if (!std::strcmp(line,"HOME")) showShell(now);
  else if (!std::strcmp(line,"BACK")) {
    if (view==View::Shell) { appManager.cancelTouch(); shellWaitForRelease=true; appManager.back(); }
    else showShell(now);
  }
  else if (!std::strcmp(line,"NEXT")) {
    if (view!=View::Diagnostics) showDiagnostics(now);
    else diagnostics.nextTest(now);
  } else if (!std::strcmp(line,"STATUS")) {
    printCalibration(); lastFocusRevision=0; logFocusState(); logMissionState(true);
  } else if (!std::strcmp(line,"MISSIONS")) logMissionState(true);
  else if (!std::strncmp(line,"QTITLE ",7)) titleCommand(line+7);
  else printHelp();
}

void serviceSerial(uint32_t now) {
  static char buffer[40];  // Fits "QTITLE 5 " + 16-char title.
  static uint8_t length=0;
  static bool overflow=false;
  // A bounded parser keeps touch polling alive even if Serial is flooded.
  for (int budget=0;budget<32 && Serial.available();++budget) {
    const int ch=Serial.read();
    if (ch=='\r' || ch=='\n') {
      if (overflow) Serial.println("SERIAL: COMMAND_TOO_LONG");
      else if (length) { buffer[length]=0; command(buffer,now); }
      length=0; overflow=false;
    } else if (ch>=32 && ch<=126) {
      if (length+1<sizeof(buffer)) buffer[length++]=std::toupper(ch);
      else overflow=true;
    }
  }
}

void logTouch(const TouchFrame& frame, uint32_t now) {
  if (!frame.updated) return;
  if (frame.rawValid && !observedRaw) {
    observedRaw=true; Serial.println("TOUCH_INPUT: FIRST_VALID_RAW (hardware acceptance still PENDING)");
  }
  if (!frame.events && !(frame.down && frame.rawValid && now-lastTouchLog>=200)) return;
  lastTouchLog=now;
  Serial.printf("TOUCH: RAW x=%d y=%d z=%d\n",frame.raw.x,frame.raw.y,frame.raw.z);
  if (haveCalibration && view!=View::Calibration) {
    const TouchPixel p=touchcal::map(calibration,frame.raw);
    Serial.printf("TOUCH: PIXEL x=%d y=%d\n",p.x,p.y);
  }
  if (frame.events&TOUCH_PRESS) Serial.println("TOUCH: EVENT PRESS");
  if (frame.events&TOUCH_RELEASE) Serial.println("TOUCH: EVENT RELEASE");
  if (frame.events&TOUCH_TAP) Serial.println("TOUCH: EVENT TAP");
  if (!frame.events && frame.heldMs>700) Serial.println("TOUCH: EVENT HOLD");
}

void setup() {
  Serial.begin(115200);
  display.begin();
  const bool configured=touch.begin();
  Serial.printf("FIRMWARE_VERSION: %s\n", FIRMWARE_VERSION);
  Serial.printf("TOUCH_INIT: %s (begin is not a hardware response test)\n",configured?"CONFIGURED":"SETUP_FAILED");
  Serial.printf("Heap: %u bytes\n", ESP.getFreeHeap());
  display.applyColorConfig();
  Serial.println("DISPLAY_CONFIG: INVERT_ON");
  focusService.begin();
  Serial.printf("FOCUS_RESTORE: result=%u running_restored_as_paused=%u focus_ms=%lu break_ms=%lu\n",
                static_cast<unsigned>(focusService.loadResult()),focusService.restoredRunning()?1U:0U,
                static_cast<unsigned long>(deskos::FOCUS_DURATION_MS),
                static_cast<unsigned long>(deskos::BREAK_DURATION_MS));
  logFocusState();
  missionService.begin();  // After Focus: the Focus anchor needs loaded progress.
  Serial.printf("MISSION_RESTORE: result=%s day=%lu bonus_xp=%lu total_xp=%lu\n",
                deskos::missionLoadName(missionService.loadResult()),
                static_cast<unsigned long>(missionService.dayId()),
                static_cast<unsigned long>(progressService.bonusXP()),
                static_cast<unsigned long>(progressService.totalXP()));
  lastMissionGrants=progressService.ledger().grantedCount;
  logMissionState(true);
  printHelp();
  registerShell();
  haveCalibration=touchcal::load(calibration);
  if (haveCalibration) {
    Serial.println("TOUCH_CAL: LOADED_FROM_NVS"); printCalibration(); showShell(millis());
  } else {
    Serial.println("TOUCH_CAL: NVS_MISSING_OR_INVALID"); startCalibration(millis());
  }
  if (view==View::Shell) appManager.update(0);  // First Home render completes before READY.
  Serial.println("DISPLAY_READY");
}

void loop() {
  const uint32_t now=millis();
  focusService.update(); // Always runs, including Home, other apps, CAL, TEST and DIAG.
  missionService.update();  // Same loop: consumes a committed FocusCompleted immediately.
  serviceSerial(now);
  const TouchFrame frame=touch.poll(now);
  logTouch(frame,now);
  if (view==View::Calibration) {
    calibrationScreen.update(frame,now);
    touchcal::CalibrationRecord completed{};
    if (calibrationScreen.takeResult(completed)) {
      if (touchcal::save(completed)) {
        calibration=completed; haveCalibration=true;
        Serial.println("TOUCH_CAL: SAVED_TO_NVS"); printCalibration(); showShell(now);
      } else calibrationScreen.storageFailed();
    }
  } else if (view==View::Test) touchTest.update(frame,calibration,now);
  else if (view==View::Diagnostics) {
    if (frame.updated) {
      if (diagnosticsWaitForRelease) {
        if (!frame.down && !frame.rawValid) diagnosticsWaitForRelease=false;
      } else if (frame.events&TOUCH_TAP) diagnostics.nextTest(now);
    }
    diagnostics.update(now);
  } else {
    dispatchShellTouch(frame);
    const uint32_t deltaMs=now-lastShellUpdate; lastShellUpdate=now;
    appManager.update(deltaMs);
  }
  logFocusState();
  logMissionState();
  if (now-lastHeapLog>=5000) {
    lastHeapLog=now;
    const char* activeApp=appManager.activeId()==deskos::AppId::None?"NONE":
                          deskos::appInfo(appManager.activeId()).label;
    Serial.printf("MEMORY: uptime_ms=%lu mode=%s app=%s switches=%lu free_heap=%u largest_free_block=%u\n",
                  static_cast<unsigned long>(now),viewName(),activeApp,
                  static_cast<unsigned long>(appManager.switchCount()),ESP.getFreeHeap(),ESP.getMaxAllocHeap());
  }
  yield();  // No timed delay; TouchHAL and diagnostics retain their millis() deadlines.
}
