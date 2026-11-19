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
#include "storage/BlobStoreNVS.h"
#include "apps/PixelPetScreen.h"
#include "apps/PixelWindowScreen.h"
#include "net/NetworkService.h"
#include "net/TimeServiceESP.h"
#include "net/HttpsFetchTask.h"
#include "services/LiveWindowContext.h"
#include "apps/WeatherScreen.h"

DisplayHAL display;
TouchHAL touch;
constexpr char FIRMWARE_VERSION[] = "M5_WEATHER_01";
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
deskos::NetworkService network;          // Wi-Fi only when configured over Serial.
deskos::TimeServiceESP timeService;      // NTP, valid only after a sync in this boot.
// Quest days: manual NEW DAY always; plus automatic local-midnight rollover once NTP is synced.
deskos::DailyMissionService missionService(focusClock,missionStorage,progressService,focusCompletions,&timeService);
deskos::DailyMissionsScreen missionsScreen(pixelRenderer,appManager,missionService,progressService);
deskos::BlobStoreNVS blobStore;  // Shared by Pet, Window and Weather records.
deskos::PetService petService(blobStore,progressService,focusClock);
deskos::PixelPetScreen petScreen(pixelRenderer,appManager,petService);
deskos::HttpsFetchTask httpsFetcher;
deskos::WeatherService weatherService(focusClock,blobStore,httpsFetcher,network,timeService);
deskos::WeatherScreen weatherScreen(pixelRenderer,appManager,weatherService,network,timeService,focusClock);
deskos::LiveWindowContext windowContext(timeService,weatherService);  // AUTO: NO CLOCK until NTP.
deskos::PixelWindowScreen windowScreen(pixelRenderer,appManager,blobStore,windowContext);

enum class View { Calibration, Test, Diagnostics, Shell };
View view=View::Calibration;
bool haveCalibration=false, diagnosticsWaitForRelease=true, observedRaw=false;
bool shellReady=false, shellWaitForRelease=true;
uint32_t lastTouchLog=0, lastHeapLog=0;
uint32_t lastShellUpdate=0;
uint32_t lastFocusRevision=0, lastMissionRevision=0, lastMissionGrants=0;
bool httpsFetcherStarted=false;

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

deskos::IApp& screenFor(deskos::AppId id) {
  switch (id) {
    case deskos::AppId::Pet: return petScreen;
    case deskos::AppId::Window: return windowScreen;
    case deskos::AppId::Weather: return weatherScreen;
    case deskos::AppId::Focus: return focusScreen;
    case deskos::AppId::Missions: return missionsScreen;
    default: return placeholderScreen;  // Apps not built yet keep the M0 placeholder.
  }
}

void registerShell() {
  shellReady=appManager.registerApp(deskos::AppId::Home,homeScreen);
  uint8_t placeholders=0;
  for (uint8_t id=1;id<deskos::APP_COUNT;++id) {
    const auto appId=static_cast<deskos::AppId>(id);
    deskos::IApp& screen=screenFor(appId);
    if (&screen==&placeholderScreen) ++placeholders;
    if (!appManager.registerApp(appId,screen)) shellReady=false;
  }
  Serial.printf("SHELL_REGISTRY: %s ids=7 dedicated_apps=%u placeholders=%u\n",shellReady?"READY":"FAILED",
                static_cast<unsigned>(deskos::APP_COUNT-1-placeholders),static_cast<unsigned>(placeholders));
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
  Serial.println("COMMANDS: HOME | BACK | CAL | RETRY | TEST | DIAG | NEXT | STATUS | MISSIONS | QTITLE <ID> <TITLE> | PET | HELP (newline)");
  Serial.println("NETWORK: WIFI | WIFI SSID <name> | WIFI PASS <password> | WIFI OPEN | WIFI ON | WIFI OFF | WIFI FORGET | TIME");
  Serial.println("WEATHER: WEATHER | WEATHER REFRESH | WEATHER LOC <lat> <lon> [label]");
}

void logPetState() {
  Serial.printf("PET: load=%u level=%u xp=%lu next=%lu style=%s pets=%lu last_seen_xp=%lu\n",
                static_cast<unsigned>(petService.loadResult()),static_cast<unsigned>(petService.level()),
                static_cast<unsigned long>(petService.xp()),static_cast<unsigned long>(petService.nextLevelXP()),
                deskos::styleName(petService.style()),static_cast<unsigned long>(petService.petCount()),
                static_cast<unsigned long>(petService.record().lastSeenXP));
}

void logNetState() {
  char ip[16]; deskos::formatIPv4(network.ipv4(),ip,sizeof(ip));
  Serial.printf("NET: state=%s ssid=\"%s\" pass_len=%u enabled=%u ip=%s rssi=%d reconnects=%lu\n",
                deskos::netStateName(network.state()),network.ssid(),static_cast<unsigned>(network.passwordLength()),
                network.enabled()?1U:0U,ip,static_cast<int>(network.rssi()),static_cast<unsigned long>(network.reconnects()));
  if (timeService.valid()) {
    const uint32_t e=timeService.epoch();
    Serial.printf("TIME: synced=1 local=%02u:%02u day_key=%lu syncs=%lu\n",static_cast<unsigned>(deskos::localHour(e)),
                  static_cast<unsigned>(deskos::localMinute(e)),static_cast<unsigned long>(deskos::localDayKey(e)),
                  static_cast<unsigned long>(timeService.syncCount()));
  } else Serial.printf("TIME: synced=0 (no NTP yet; quest days stay manual)\n");
}

uint32_t lastWeatherRevision=0;
void logWeatherState(bool force=false) {
  if (!force && lastWeatherRevision==weatherService.revision()) return;
  lastWeatherRevision=weatherService.revision();
  const auto& r=weatherService.reading();
  uint32_t age=0; const bool known=weatherService.ageSeconds(age);
  char lat[16],lon[16];
  deskos::formatCoordinate(weatherService.location().latE4,lat,sizeof(lat));
  deskos::formatCoordinate(weatherService.location().lonE4,lon,sizeof(lon));
  Serial.printf("WEATHER: loc=\"%s\" %s,%s has=%u src=%u temp_deci=%d code=%u hum=%u age_s=%ld fetching=%u error=%s http=%d failures=%u next_ms=%lu\n",
                weatherService.location().label,lat,lon,weatherService.hasReading()?1U:0U,
                static_cast<unsigned>(weatherService.source()),static_cast<int>(r.temperatureDeci),static_cast<unsigned>(r.code),
                static_cast<unsigned>(r.humidity),known?static_cast<long>(age):-1L,weatherService.fetching()?1U:0U,
                deskos::weatherErrorName(weatherService.lastError()),weatherService.lastHttpCode(),
                static_cast<unsigned>(weatherService.failures()),static_cast<unsigned long>(weatherService.msUntilNextAttempt()));
}

// WIFI SSID <name> | WIFI PASS <password> | WIFI OPEN | WIFI ON | WIFI OFF | WIFI FORGET | WIFI
// The password is case-sensitive, stored only in NVS and never printed back.
void wifiCommand(const char* upper,const char* raw) {
  bool ok=true;
  if (!std::strncmp(upper,"WIFI SSID ",10)) ok=network.setSsid(raw+10);
  else if (!std::strncmp(upper,"WIFI PASS ",10)) ok=network.setPassword(raw+10);
  else if (!std::strcmp(upper,"WIFI OPEN")) ok=network.setPassword("");
  else if (!std::strcmp(upper,"WIFI ON")) network.setEnabled(true);
  else if (!std::strcmp(upper,"WIFI OFF")) network.setEnabled(false);
  else if (!std::strcmp(upper,"WIFI FORGET")) network.forget();
  else if (std::strcmp(upper,"WIFI")) ok=false;
  if (!ok) Serial.println("WIFI: REJECTED (SSID 1-32 chars, password 8-64 chars or WIFI OPEN)");
  logNetState();
}

// WEATHER | WEATHER REFRESH | WEATHER LOC <lat> <lon> [LABEL]
void weatherCommand(const char* upper) {
  if (!std::strcmp(upper,"WEATHER REFRESH")) {
    Serial.println(weatherService.requestRefresh()?"WEATHER: REFRESH_QUEUED":"WEATHER: BUSY_OR_RATE_LIMITED (30 s)");
  } else if (!std::strncmp(upper,"WEATHER LOC ",12)) {
    char lat[16]={0},lon[16]={0},label[16]={0};
    const int n=std::sscanf(upper+12,"%15s %15s %15[A-Z0-9 .,-]",lat,lon,label);
    deskos::WeatherLocation l;
    std::memset(&l,0,sizeof(l));
    std::snprintf(l.label,sizeof(l.label),"%s",n>=3?label:"CUSTOM");
    const bool ok=n>=2 && deskos::parseCoordinateE4(lat,900000,l.latE4) &&
                  deskos::parseCoordinateE4(lon,1800000,l.lonE4) && weatherService.setLocation(l);
    Serial.println(ok?"WEATHER: LOCATION_SAVED":"WEATHER: USAGE WEATHER LOC <LAT> <LON> [LABEL<=15]");
  } else if (std::strcmp(upper,"WEATHER")) Serial.println("WEATHER: USAGE WEATHER | WEATHER REFRESH | WEATHER LOC <LAT> <LON> [LABEL]");
  logWeatherState(true);
}

void command(const char* raw, uint32_t now) {
  char line[112];
  size_t n=0;
  for (;raw[n] && n+1<sizeof(line);++n) line[n]=static_cast<char>(std::toupper(static_cast<unsigned char>(raw[n])));
  line[n]=0;
  if (!std::strcmp(line,"WIFI") || !std::strncmp(line,"WIFI ",5)) { wifiCommand(line,raw); return; }
  if (!std::strcmp(line,"WEATHER") || !std::strncmp(line,"WEATHER ",8)) { weatherCommand(line); return; }
  if (!std::strcmp(line,"TIME")) { logNetState(); return; }
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
    printCalibration(); lastFocusRevision=0; logFocusState(); logMissionState(true); logPetState();
    logNetState(); logWeatherState(true);
  } else if (!std::strcmp(line,"MISSIONS")) logMissionState(true);
  else if (!std::strcmp(line,"PET")) logPetState();
  else if (!std::strncmp(line,"QTITLE ",7)) titleCommand(line+7);
  else printHelp();
}

void serviceSerial(uint32_t now) {
  static char buffer[112];  // Raw case kept: "WIFI PASS " + 64-char passphrase fits.
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
      if (length+1<sizeof(buffer)) buffer[length++]=static_cast<char>(ch);
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
  petService.begin();  // Cosmetic record only; level is derived from shared XP.
  logPetState();
  windowScreen.begin();
  Serial.printf("WINDOW_RESTORE: load=%u scene=%s speed=%s\n",static_cast<unsigned>(windowScreen.loadResult()),
                deskos::sceneName(windowScreen.selected()),deskos::speedName(windowScreen.speed()));
  weatherService.begin();  // Loads location + last good reading (offline fallback).
  logWeatherState(true);
  network.begin();         // Non-blocking; the Wi-Fi driver starts only if credentials exist.
  logNetState();
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
  network.update(now);      // Status polling only; never waits for Wi-Fi.
  if (network.connected()) {
    if (!timeService.started()) timeService.start();
    if (!httpsFetcherStarted) {
      httpsFetcherStarted=httpsFetcher.begin();  // Background HTTPS task, created once on first IP.
      Serial.printf("HTTPS_TASK: %s stack=%lu\n",httpsFetcherStarted?"STARTED":"CREATE_FAILED",
                    static_cast<unsigned long>(deskos::HttpsFetchTask::STACK_BYTES));
    }
  }
  missionService.update();  // Same loop: consumes a committed FocusCompleted immediately.
  weatherService.update();  // Polls the HTTPS task; scheduling/backoff/timeout only.
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
  logWeatherState();
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
