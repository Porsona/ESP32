#include "PcLinkUDP.h"
#include <Arduino.h>
#include <esp_random.h>

namespace deskos {
void EspRandomSource::fill(uint8_t* out,size_t length) { esp_fill_random(out,length); }

void PcLinkUDP::update() {
  if (!network_.connected()) {
    if (listening_) { udp_.stop(); listening_=false; Serial.println("PC_LINK: STOPPED (Wi-Fi down)"); }
    return;
  }
  if (!listening_) {
    listening_=udp_.begin(PC_MONITOR_PORT)==1;
    Serial.printf("PC_LINK: %s udp_port=%u\n",listening_?"LISTENING":"BIND_FAILED",static_cast<unsigned>(PC_MONITOR_PORT));
    if (!listening_) return;
  }
  // Bounded work per loop: at most 4 datagrams, each <=400 bytes.
  for (uint8_t i=0;i<4;++i) {
    const int size=udp_.parsePacket();
    if (size<=0) break;
    if (static_cast<size_t>(size)>PC_PACKET_MAX) { udp_.flush(); ++dropped_; continue; }
    const int n=udp_.read(buffer_,PC_PACKET_MAX);
    if (n<=0) continue;
    buffer_[n]=0;
    const PcRx result=monitor_.receive(buffer_,static_cast<size_t>(n),static_cast<uint32_t>(udp_.remoteIP()),
                                       network_.ipv4(),network_.subnetMask());
    if (result!=PcRx::Accepted && monitor_.rejected()%20==1)  // Rate-limited log of rejects.
      Serial.printf("PC_LINK: REJECT reason=%s from=%s total=%lu\n",pcRxName(result),
                    udp_.remoteIP().toString().c_str(),static_cast<unsigned long>(monitor_.rejected()));
  }
}
}  // namespace deskos
