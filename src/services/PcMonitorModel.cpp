#include "PcMonitorModel.h"
#include <string.h>
#include "Crc32.h"
#include "Sha256.h"

namespace deskos {
namespace {
constexpr uint32_t TOKEN_MAGIC=0x50434B31UL;    // "PCK1"
constexpr uint32_t COUNTER_MAGIC=0x50434E31UL;  // "PCN1"
int hexValue(char c) {
  if (c>='0' && c<='9') return c-'0';
  if (c>='a' && c<='f') return c-'a'+10;
  if (c>='A' && c<='F') return c-'A'+10;
  return -1;
}
bool parseInt(const char* p,size_t n,int64_t minimum,int64_t maximum,int64_t& out) {
  if (n==0 || n>20) return false;
  bool negative=false; size_t i=0;
  if (p[0]=='-') { negative=true; i=1; if (n==1) return false; }
  uint64_t v=0;
  for (;i<n;++i) {
    if (p[i]<'0' || p[i]>'9') return false;
    if (v>(UINT64_MAX-9)/10) return false;
    v=v*10+static_cast<uint64_t>(p[i]-'0');
  }
  if (v>static_cast<uint64_t>(INT64_MAX)) return false;
  const int64_t s=negative?-static_cast<int64_t>(v):static_cast<int64_t>(v);
  if (s<minimum || s>maximum) return false;
  out=s; return true;
}
bool keyIs(const char* key,size_t n,const char* name) { return strlen(name)==n && !memcmp(key,name,n); }
}  // namespace

const char* pcResultName(PcPacketResult r) { return r==PcPacketResult::Ok?"OK":r==PcPacketResult::Auth?"AUTH":"FORMAT"; }

void tokenToHex(const uint8_t (&t)[PC_TOKEN_BYTES],char (&out)[PC_TOKEN_BYTES*2+1]) {
  static const char HEX[]="0123456789abcdef";
  for (size_t i=0;i<PC_TOKEN_BYTES;++i) { out[2*i]=HEX[t[i]>>4]; out[2*i+1]=HEX[t[i]&15]; }
  out[PC_TOKEN_BYTES*2]=0;
}
bool tokenFromHex(const char* hex,uint8_t (&t)[PC_TOKEN_BYTES]) {
  if (!hex || strlen(hex)!=PC_TOKEN_BYTES*2) return false;
  for (size_t i=0;i<PC_TOKEN_BYTES;++i) {
    const int hi=hexValue(hex[2*i]),lo=hexValue(hex[2*i+1]);
    if (hi<0 || lo<0) return false;
    t[i]=static_cast<uint8_t>(hi<<4|lo);
  }
  return true;
}

PcPacketResult parsePcPacket(const char* data,size_t length,const uint8_t (&token)[PC_TOKEN_BYTES],PcStats& out) {
  if (!data || length<5 || length>PC_PACKET_MAX || memcmp(data,"DPM1;",5)) return PcPacketResult::Format;
  for (size_t i=0;i<length;++i) if (data[i]<32 || data[i]>126) return PcPacketResult::Format;
  // The MAC field must be last and exactly 64 hex digits.
  if (length<5+4+64 || memcmp(data+length-68,"mac=",4) || data[length-69]!=';') return PcPacketResult::Format;
  uint8_t claimed[32];
  for (size_t i=0;i<32;++i) {
    const int hi=hexValue(data[length-64+2*i]),lo=hexValue(data[length-64+2*i+1]);
    if (hi<0 || lo<0) return PcPacketResult::Format;
    claimed[i]=static_cast<uint8_t>(hi<<4|lo);
  }
  const size_t signedLength=length-68;
  uint8_t expected[32];
  hmacSha256(token,PC_TOKEN_BYTES,data,signedLength,expected);
  // Authenticate before interpreting any field.
  if (!equalConstantTime(expected,claimed,32)) return PcPacketResult::Auth;

  PcStats s;
  memset(&s,0,sizeof(s));
  uint16_t seen=0;
  const uint16_t REQUIRED=0x1FFF;
  size_t pos=5;
  while (pos<signedLength) {
    const char* field=data+pos;
    const char* end=static_cast<const char*>(memchr(field,';',signedLength-pos));
    if (!end) return PcPacketResult::Format;
    const char* eq=static_cast<const char*>(memchr(field,'=',end-field));
    if (!eq) return PcPacketResult::Format;
    const size_t keyLength=eq-field,valueLength=end-eq-1;
    const char* value=eq+1;
    int64_t v=0;
    uint16_t bit=0;
    if (keyIs(field,keyLength,"n")) { if (!parseInt(value,valueLength,1,INT64_MAX,v)) return PcPacketResult::Format; s.counter=static_cast<uint64_t>(v); bit=1; }
    else if (keyIs(field,keyLength,"t")) { if (!parseInt(value,valueLength,0,UINT32_MAX,v)) return PcPacketResult::Format; s.unixTime=static_cast<uint32_t>(v); bit=2; }
    else if (keyIs(field,keyLength,"h")) {
      if (valueLength==0 || valueLength>15) return PcPacketResult::Format;
      for (size_t i=0;i<valueLength;++i) {
        char c=value[i];
        if (c>='a' && c<='z') c=static_cast<char>(c-'a'+'A');
        if (!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.')) return PcPacketResult::Format;
        s.host[i]=c;
      }
      bit=4;
    }
    else if (keyIs(field,keyLength,"cpu")) { if (!parseInt(value,valueLength,0,1000,v)) return PcPacketResult::Format; s.cpuPermille=static_cast<uint16_t>(v); bit=8; }
    else if (keyIs(field,keyLength,"ram")) { if (!parseInt(value,valueLength,0,1000,v)) return PcPacketResult::Format; s.ramPermille=static_cast<uint16_t>(v); bit=16; }
    else if (keyIs(field,keyLength,"ru")) { if (!parseInt(value,valueLength,0,16777216,v)) return PcPacketResult::Format; s.ramUsedMB=static_cast<uint32_t>(v); bit=32; }
    else if (keyIs(field,keyLength,"rt")) { if (!parseInt(value,valueLength,0,16777216,v)) return PcPacketResult::Format; s.ramTotalMB=static_cast<uint32_t>(v); bit=64; }
    else if (keyIs(field,keyLength,"dsk")) { if (!parseInt(value,valueLength,0,1000,v)) return PcPacketResult::Format; s.diskPermille=static_cast<uint16_t>(v); bit=128; }
    else if (keyIs(field,keyLength,"up")) { if (!parseInt(value,valueLength,-1,100000000,v)) return PcPacketResult::Format; s.upKBps=static_cast<int32_t>(v); bit=256; }
    else if (keyIs(field,keyLength,"dn")) { if (!parseInt(value,valueLength,-1,100000000,v)) return PcPacketResult::Format; s.downKBps=static_cast<int32_t>(v); bit=512; }
    else if (keyIs(field,keyLength,"bat")) { if (!parseInt(value,valueLength,-1,100,v)) return PcPacketResult::Format; s.batteryPercent=static_cast<int8_t>(v); bit=1024; }
    else if (keyIs(field,keyLength,"chg")) { if (!parseInt(value,valueLength,0,1,v)) return PcPacketResult::Format; s.charging=static_cast<uint8_t>(v); bit=2048; }
    else return PcPacketResult::Format;  // Unknown key: reject rather than guess.
    if (seen&bit) return PcPacketResult::Format;  // Duplicate key.
    seen|=bit; pos=static_cast<size_t>(end-data)+1;
  }
  seen|=4096;  // mac (checked above)
  if (seen!=REQUIRED || s.ramUsedMB>s.ramTotalMB) return PcPacketResult::Format;
  out=s;
  return PcPacketResult::Ok;
}

PcTokenRecord makePcTokenRecord(const uint8_t (&token)[PC_TOKEN_BYTES]) {
  PcTokenRecord r;
  memset(&r,0,sizeof(r));
  r.magic=TOKEN_MAGIC; r.version=PC_TOKEN_SCHEMA_VERSION; r.bytes=sizeof(r);
  memcpy(r.token,token,PC_TOKEN_BYTES);
  r.checksum=crc32Bytes(&r,offsetof(PcTokenRecord,checksum));
  return r;
}
bool validPcTokenRecord(const PcTokenRecord& r) {
  uint8_t any=0;
  for (uint8_t b:r.token) any|=b;
  return r.magic==TOKEN_MAGIC && r.version==PC_TOKEN_SCHEMA_VERSION && r.bytes==sizeof(r) && any &&
         r.checksum==crc32Bytes(&r,offsetof(PcTokenRecord,checksum));
}

PcCounterRecord makePcCounterRecord(const uint8_t (&token)[PC_TOKEN_BYTES],uint64_t counter) {
  PcCounterRecord r;
  memset(&r,0,sizeof(r));
  r.magic=COUNTER_MAGIC; r.version=PC_COUNTER_SCHEMA_VERSION; r.bytes=sizeof(r);
  r.tokenTag=crc32Bytes(token,PC_TOKEN_BYTES);
  r.counterLow=static_cast<uint32_t>(counter); r.counterHigh=static_cast<uint32_t>(counter>>32);
  r.checksum=crc32Bytes(&r,offsetof(PcCounterRecord,checksum));
  return r;
}
bool readPcCounterRecord(const PcCounterRecord& r,const uint8_t (&token)[PC_TOKEN_BYTES],uint64_t& counter) {
  if (r.magic!=COUNTER_MAGIC || r.version!=PC_COUNTER_SCHEMA_VERSION || r.bytes!=sizeof(r) ||
      r.checksum!=crc32Bytes(&r,offsetof(PcCounterRecord,checksum)) || r.tokenTag!=crc32Bytes(token,PC_TOKEN_BYTES))
    return false;
  const uint64_t value=static_cast<uint64_t>(r.counterHigh)<<32|r.counterLow;
  if (!value) return false;
  counter=value;
  return true;
}
}  // namespace deskos
