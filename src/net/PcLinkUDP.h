#pragma once
#include <WiFiUdp.h>
#include "../services/NetTypes.h"
#include "../services/PcMonitorService.h"

namespace deskos {
// Hardware RNG (true random while the RF subsystem is on).
class EspRandomSource : public IRandomSource {
 public:
  void fill(uint8_t* out,size_t length) override;
};

// Receive-only UDP socket on the station interface while Wi-Fi is up. No
// replies are sent and no payload is ever executed; datagrams go to the parser.
class PcLinkUDP {
 public:
  PcLinkUDP(PcMonitorService& monitor,const INetworkStatus& network) : monitor_(monitor),network_(network) {}
  void update();
  bool listening() const { return listening_; }
  uint32_t dropped() const { return dropped_; }
 private:
  PcMonitorService& monitor_;
  const INetworkStatus& network_;
  WiFiUDP udp_;
  bool listening_=false;
  uint32_t dropped_=0;
  char buffer_[PC_PACKET_MAX+1]={0};
};
}  // namespace deskos
