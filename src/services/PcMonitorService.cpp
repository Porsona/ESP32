#include "PcMonitorService.h"
#include <string.h>

namespace deskos {
namespace { const char* const NS="deskos-pc"; const char* const KEY="token"; const char* const FLOOR_KEY="ctr"; }

const char* pcLinkName(PcLinkState s) {
  switch (s) {
    case PcLinkState::NoNetwork: return "NO WIFI";
    case PcLinkState::NoToken: return "NO TOKEN";
    case PcLinkState::Waiting: return "WAITING";
    case PcLinkState::Connected: return "CONNECTED";
    default: return "DISCONNECTED";
  }
}
const char* pcRxName(PcRx r) {
  switch (r) {
    case PcRx::Accepted: return "ACCEPTED";
    case PcRx::Format: return "FORMAT";
    case PcRx::Auth: return "AUTH";
    case PcRx::Replay: return "REPLAY";
    case PcRx::Stale: return "CLOCK_SKEW";
    case PcRx::Source: return "NOT_LAN";
    case PcRx::RateLimited: return "RATE_LIMIT";
    default: return "NO_TOKEN";
  }
}

void PcMonitorService::begin() {
  PcTokenRecord r;
  if (store_.load(NS,KEY,&r,sizeof(r))==BlobLoad::Loaded && validPcTokenRecord(r)) {
    memcpy(token_,r.token,PC_TOKEN_BYTES); hasToken_=true;
    PcCounterRecord c;
    uint64_t floor=0;
    if (store_.load(NS,FLOOR_KEY,&c,sizeof(c))==BlobLoad::Loaded && readPcCounterRecord(c,token_,floor))
      lastCounter_=savedFloor_=floor;  // Packets captured before the last checkpoint stay rejected.
  }
}
void PcMonitorService::checkpointCounter(uint32_t now) {
  if (lastCounter_<=savedFloor_) return;
  if (floorSaveTried_ && now-lastFloorSaveMs_<FLOOR_SAVE_MS) return;
  floorSaveTried_=true; lastFloorSaveMs_=now;  // Throttled even when the write fails.
  const PcCounterRecord r=makePcCounterRecord(token_,lastCounter_);
  if (store_.save(NS,FLOOR_KEY,&r,sizeof(r))) savedFloor_=lastCounter_;
}
bool PcMonitorService::regenerateToken() {
  uint8_t fresh[PC_TOKEN_BYTES];
  uint8_t any=0;
  for (int attempt=0;attempt<4 && !any;++attempt) { random_.fill(fresh,PC_TOKEN_BYTES); for (uint8_t b:fresh) any|=b; }
  if (!any) return false;
  const PcTokenRecord r=makePcTokenRecord(fresh);
  if (!store_.save(NS,KEY,&r,sizeof(r))) return false;  // Only switch after the token is durable.
  memcpy(token_,fresh,PC_TOKEN_BYTES); hasToken_=true;
  // The old floor's token tag no longer matches, so it is ignored without a write.
  lastCounter_=savedFloor_=0; floorSaveTried_=false; hasStats_=false; ++revision_;
  return true;
}
bool PcMonitorService::ensureToken() { return hasToken_ || regenerateToken(); }
void PcMonitorService::tokenHex(char (&out)[PC_TOKEN_BYTES*2+1]) const {
  if (hasToken_) tokenToHex(token_,out); else out[0]=0;
}

PcRx PcMonitorService::reject(PcRx reason) {
  ++rejected_; lastReject_=reason; return reason;
}
PcRx PcMonitorService::receive(const char* data,size_t length,uint32_t source,uint32_t localIp,uint32_t mask) {
  const uint32_t now=clock_.nowMs();
  if (now-windowStart_>=1000) { windowStart_=now; windowCount_=0; }
  if (windowCount_>=MAX_PER_SECOND) return reject(PcRx::RateLimited);  // Before any HMAC work.
  ++windowCount_;
  if (!hasToken_) return reject(PcRx::NoToken);
  if (!source || !mask || (source&mask)!=(localIp&mask) || source==localIp) return reject(PcRx::Source);
  PcStats parsed;
  const PcPacketResult result=parsePcPacket(data,length,token_,parsed);
  if (result==PcPacketResult::Auth) return reject(PcRx::Auth);
  if (result!=PcPacketResult::Ok) return reject(PcRx::Format);
  if (parsed.counter<=lastCounter_) return reject(PcRx::Replay);
  if (time_.valid()) {
    const uint32_t e=time_.epoch();
    const uint32_t skew=e>parsed.unixTime?e-parsed.unixTime:parsed.unixTime-e;
    if (skew>MAX_SKEW_S) return reject(PcRx::Stale);
  }
  stats_=parsed; lastCounter_=parsed.counter; lastValidMs_=now; hasStats_=true; ++accepted_; ++revision_;
  if (time_.valid()) checkpointCounter(now);
  return PcRx::Accepted;
}
PcLinkState PcMonitorService::state(bool networkUp) const {
  if (!networkUp) return PcLinkState::NoNetwork;
  if (!hasToken_) return PcLinkState::NoToken;
  if (!hasStats_) return PcLinkState::Waiting;
  return clock_.nowMs()-lastValidMs_<TIMEOUT_MS?PcLinkState::Connected:PcLinkState::Disconnected;
}
}  // namespace deskos
