#include "FocusModel.h"

namespace deskos {
namespace { constexpr uint32_t MAGIC=0x46515331UL; }
uint32_t focusDuration(FocusMode mode) { return mode==FocusMode::Focus?FOCUS_DURATION_MS:BREAK_DURATION_MS; }
uint32_t focusChecksum(const FocusRecord& r) {
  const auto* bytes=reinterpret_cast<const uint8_t*>(&r);
  uint32_t crc=0xFFFFFFFFUL;
  for (size_t i=0;i<offsetof(FocusRecord,checksum);++i) {
    crc^=bytes[i];
    for (int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xEDB88320UL:0);
  }
  return ~crc;
}
void sealFocusRecord(FocusRecord& r) { r.checksum=focusChecksum(r); }
FocusRecord defaultFocusRecord() {
  FocusRecord r{};
  r.magic=MAGIC; r.version=1; r.bytes=sizeof(r);
  r.mode=FocusMode::Focus; r.state=FocusState::Idle; r.remainingMs=FOCUS_DURATION_MS;
  sealFocusRecord(r); return r;
}
bool validFocusRecord(const FocusRecord& r) {
  if (r.magic!=MAGIC || r.version!=1 || r.bytes!=sizeof(r) || r.reserved[0] || r.reserved[1] ||
      static_cast<uint8_t>(r.mode)>1 || static_cast<uint8_t>(r.state)>3 ||
      r.checksum!=focusChecksum(r) || !ProgressService::valid(r.progress)) return false;
  if (r.remainingMs==0 || r.remainingMs>focusDuration(r.mode)) return false;
  if (r.state==FocusState::Idle || r.state==FocusState::Completed) {
    if (r.remainingMs!=focusDuration(r.mode)) return false;
  }
  if (r.state==FocusState::Running || r.state==FocusState::Paused) {
    if (!r.progress.currentSessionId) return false;
    if (r.mode==FocusMode::Focus && r.progress.currentSessionId<=r.progress.lastRewardedSessionId) return false;
  }
  if (r.mode==FocusMode::Break || r.state==FocusState::Completed) {
    if (!r.progress.completedFocusSessions ||
        r.progress.currentSessionId!=r.progress.lastRewardedSessionId) return false;
  }
  return true;
}
const char* focusStateName(FocusState s) {
  switch (s) {
    case FocusState::Idle: return "IDLE";
    case FocusState::Running: return "RUNNING";
    case FocusState::Paused: return "PAUSED";
    case FocusState::Completed: return "COMPLETED";
    default: return "INVALID";
  }
}
const char* focusModeName(FocusMode m) { return m==FocusMode::Focus?"FOCUS":"BREAK"; }
}  // namespace deskos
