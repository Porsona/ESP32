#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Protocol DPM1 (one UDP datagram, ASCII, LAN only, PC -> ESP32 only):
//   DPM1;n=<counter>;t=<unix>;h=<host>;cpu=<permille>;ram=<permille>;ru=<MB>;rt=<MB>;
//   dsk=<permille>;up=<KB/s|-1>;dn=<KB/s|-1>;bat=<0..100|-1>;chg=<0|1>;mac=<64 hex>
// mac = HMAC-SHA256(token, every byte before "mac="). The ESP32 only parses
// numbers into a fixed struct: there are no commands, so nothing can execute.
constexpr uint16_t PC_MONITOR_PORT=47801;
constexpr size_t PC_PACKET_MAX=400;
constexpr size_t PC_TOKEN_BYTES=16;
struct PcStats {
  uint64_t counter;
  uint32_t unixTime,ramUsedMB,ramTotalMB;
  int32_t upKBps,downKBps;     // -1 = not available
  uint16_t cpuPermille,ramPermille,diskPermille;
  int8_t batteryPercent;       // -1 = no battery
  uint8_t charging;
  char host[16];
};
enum class PcPacketResult : uint8_t { Ok, Format, Auth };
PcPacketResult parsePcPacket(const char* data,size_t length,const uint8_t (&token)[PC_TOKEN_BYTES],PcStats& out);
const char* pcResultName(PcPacketResult result);
void tokenToHex(const uint8_t (&token)[PC_TOKEN_BYTES],char (&out)[PC_TOKEN_BYTES*2+1]);
bool tokenFromHex(const char* hex,uint8_t (&token)[PC_TOKEN_BYTES]);

constexpr uint16_t PC_TOKEN_SCHEMA_VERSION=1;
struct PcTokenRecord {
  uint32_t magic;
  uint16_t version,bytes;
  uint8_t token[PC_TOKEN_BYTES];
  uint32_t checksum;
};
static_assert(sizeof(PcTokenRecord)==28,"Change PC token schema version when the layout changes");
PcTokenRecord makePcTokenRecord(const uint8_t (&token)[PC_TOKEN_BYTES]);
bool validPcTokenRecord(const PcTokenRecord& record);
}  // namespace deskos
