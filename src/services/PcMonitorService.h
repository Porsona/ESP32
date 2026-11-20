#pragma once
#include "BlobStore.h"
#include "FocusStorage.h"
#include "PcMonitorModel.h"
#include "TimeModel.h"

namespace deskos {
class IRandomSource {
 public:
  virtual ~IRandomSource() = default;
  virtual void fill(uint8_t* out,size_t length) = 0;
};
enum class PcLinkState : uint8_t { NoNetwork, NoToken, Waiting, Connected, Disconnected };
enum class PcRx : uint8_t { Accepted, Format, Auth, Replay, Stale, Source, RateLimited, NoToken };
const char* pcLinkName(PcLinkState state);
const char* pcRxName(PcRx result);

// Receive-only telemetry sink. Checks are ordered cheapest first: rate limit,
// same-subnet source, then HMAC, then replay counter and clock skew.
class PcMonitorService {
 public:
  static constexpr uint32_t TIMEOUT_MS=6000;     // Agent sends every 2 s: 3 missed packets.
  static constexpr uint32_t MAX_SKEW_S=300;      // Only when both clocks are trustworthy.
  static constexpr uint8_t MAX_PER_SECOND=20;
  PcMonitorService(IBlobStore& store,IFocusClock& clock,const ITimeSource& time,IRandomSource& random)
      : store_(store),clock_(clock),time_(time),random_(random) {}
  void begin();
  bool ensureToken();        // Generates + saves a 128-bit token once (call with RF/Wi-Fi on).
  bool regenerateToken();    // Old agents stop authenticating immediately.
  bool hasToken() const { return hasToken_; }
  void tokenHex(char (&out)[PC_TOKEN_BYTES*2+1]) const;
  PcRx receive(const char* data,size_t length,uint32_t source,uint32_t localIp,uint32_t mask);
  PcLinkState state(bool networkUp) const;
  bool hasStats() const { return hasStats_; }
  const PcStats& stats() const { return stats_; }
  uint32_t secondsSinceLast() const { return hasStats_?(clock_.nowMs()-lastValidMs_)/1000:0; }
  uint32_t accepted() const { return accepted_; }
  uint32_t rejected() const { return rejected_; }
  PcRx lastReject() const { return lastReject_; }
  uint32_t revision() const { return revision_; }
 private:
  PcRx reject(PcRx reason);
  IBlobStore& store_;
  IFocusClock& clock_;
  const ITimeSource& time_;
  IRandomSource& random_;
  uint8_t token_[PC_TOKEN_BYTES]={0};
  bool hasToken_=false,hasStats_=false;
  PcStats stats_{};
  uint64_t lastCounter_=0;
  uint32_t lastValidMs_=0,windowStart_=0,accepted_=0,rejected_=0,revision_=0;
  uint8_t windowCount_=0;
  PcRx lastReject_=PcRx::Accepted;
};
}  // namespace deskos
