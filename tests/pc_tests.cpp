#define DESKOS_TEST_ALLOC_COUNTER
#include <Arduino.h>
#include <string>
#include "TestSupport.h"
#include "apps/PcMonitorScreen.h"
#include "services/Sha256.h"

uint32_t testNow=0;
TestSerial Serial;
TestESP ESP;
using namespace deskos;
namespace {
int failures=0;
const char* g_agentPacketFile=nullptr;
const uint8_t TOKEN[16]={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
const uint32_t LOCAL=0x3201A8C0UL,PEER=0x0A01A8C0UL,MASK=0x00FFFFFFUL,OUTSIDE=0x08080808UL;  // lwIP order.

std::string hex(const uint8_t* d,size_t n) {
  static const char H[]="0123456789abcdef"; std::string s;
  for (size_t i=0;i<n;++i) { s+=H[d[i]>>4]; s+=H[d[i]&15]; }
  return s;
}
std::string sign(const std::string& body,const uint8_t* key=TOKEN) {
  uint8_t mac[32]; hmacSha256(key,16,body.data(),body.size(),mac);
  return body+"mac="+hex(mac,32);
}
std::string body(uint64_t n,uint32_t t=1763600000UL,const char* extra="") {
  char b[300];
  std::snprintf(b,sizeof(b),"DPM1;n=%llu;t=%lu;h=laptop-01;cpu=456;ram=623;ru=10240;rt=16384;dsk=711;up=12;dn=340;bat=87;chg=1;%s",
                static_cast<unsigned long long>(n),static_cast<unsigned long>(t),extra);
  return b;
}
struct FakeTime : ITimeSource {
  bool ok=false; uint32_t now=1763600000UL;
  bool valid() const override { return ok; }
  uint32_t epoch() const override { return now; }
};
struct FakeRandom : IRandomSource {
  uint8_t next=1; bool zeros=false;
  void fill(uint8_t* out,size_t n) override { for (size_t i=0;i<n;++i) out[i]=zeros?0:next++; }
};
struct FakeNet : INetworkStatus {
  NetState s=NetState::Connected;
  NetState state() const override { return s; }
  uint32_t ipv4() const override { return LOCAL; }
  uint32_t subnetMask() const override { return MASK; }
  int8_t rssi() const override { return -55; }
};
struct Fixture {
  TestClock clock; FakeBlobStore store; FakeTime time; FakeRandom random; PcMonitorService monitor;
  Fixture() : monitor(store,clock,time,random) {
    monitor.begin();
    const PcTokenRecord r=makePcTokenRecord(TOKEN);
    store.save("deskos-pc","token",&r,sizeof(r)); monitor.begin();
  }
  PcRx send(const std::string& p,uint32_t from=PEER) { return monitor.receive(p.data(),p.size(),from,LOCAL,MASK); }
};

void shaVectors() {
  uint8_t d[32]; Sha256 h;
  h.update("abc",3); h.finish(d);
  CHECK(hex(d,32)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  h.finish(d); CHECK(hex(d,32)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  const char* long56="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  h.update(long56,std::strlen(long56)); h.finish(d);
  CHECK(hex(d,32)=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  char block[1000]; std::memset(block,'a',sizeof(block));
  for (int i=0;i<1000;++i) h.update(block,sizeof(block));
  h.finish(d); CHECK(hex(d,32)=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");  // 10^6 x 'a'
  uint8_t key1[20]; std::memset(key1,0x0b,sizeof(key1));
  hmacSha256(key1,20,"Hi There",8,d);
  CHECK(hex(d,32)=="b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");  // RFC 4231 #1
  hmacSha256(reinterpret_cast<const uint8_t*>("Jefe"),4,"what do ya want for nothing?",28,d);
  CHECK(hex(d,32)=="5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");  // RFC 4231 #2
  uint8_t key6[131]; std::memset(key6,0xaa,sizeof(key6));
  const char* m6="Test Using Larger Than Block-Size Key - Hash Key First";
  hmacSha256(key6,131,m6,std::strlen(m6),d);
  CHECK(hex(d,32)=="60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");  // RFC 4231 #6
}
void packetParsing() {
  PcStats s;
  const std::string good=sign(body(42));
  CHECK(parsePcPacket(good.data(),good.size(),TOKEN,s)==PcPacketResult::Ok);
  CHECK(s.counter==42 && s.cpuPermille==456 && s.ramUsedMB==10240 && s.downKBps==340 && s.batteryPercent==87);
  CHECK(!std::strcmp(s.host,"LAPTOP-01") && s.charging==1);
  std::string tampered=good; tampered[tampered.find("cpu=456")+4]='9';
  CHECK(parsePcPacket(tampered.data(),tampered.size(),TOKEN,s)==PcPacketResult::Auth);
  uint8_t other[16]; std::memcpy(other,TOKEN,16); other[0]^=1;
  CHECK(parsePcPacket(good.data(),good.size(),other,s)==PcPacketResult::Auth);
  const std::string bad[]={
    sign(body(0)),                                            // Counter must be >=1.
    sign(body(1,1,"cmd=reboot;")),                            // Unknown key: rejected, never executed.
    sign(body(1,1,"cpu=1;")),                                 // Duplicate key.
    sign("DPM1;n=1;t=1;h=x;cpu=1001;ram=1;ru=1;rt=2;dsk=1;up=1;dn=1;bat=1;chg=0;"),
    sign("DPM1;n=1;t=1;h=x;cpu=1;ram=1;ru=3;rt=2;dsk=1;up=1;dn=1;bat=1;chg=0;"),   // used > total
    sign("DPM1;n=1;t=1;h=bad host;cpu=1;ram=1;ru=1;rt=2;dsk=1;up=1;dn=1;bat=1;chg=0;"),
    sign("DPM1;n=1;t=1;h=x;cpu=1;ram=1;ru=1;rt=2;dsk=1;up=1;dn=1;bat=1;"),          // Missing chg.
    sign("DPM2;n=1;t=1;h=x;cpu=1;ram=1;ru=1;rt=2;dsk=1;up=1;dn=1;bat=1;chg=0;"),
    good.substr(0,good.size()-1), good+"0", std::string(500,'A'), ""
  };
  for (const auto& b:bad) CHECK(parsePcPacket(b.data(),b.size(),TOKEN,s)!=PcPacketResult::Ok);
  std::string withNul=good; withNul[10]='\0';
  CHECK(parsePcPacket(withNul.data(),withNul.size(),TOKEN,s)!=PcPacketResult::Ok);
  char t[33]; tokenToHex(TOKEN,t); CHECK(!std::strcmp(t,"00112233445566778899aabbccddeeff"));
  uint8_t back[16]; CHECK(tokenFromHex(t,back) && !std::memcmp(back,TOKEN,16) && !tokenFromHex("00",back));
}
void crossCheckPythonAgent() {
  CHECK(g_agentPacketFile!=nullptr);
  FILE* f=std::fopen(g_agentPacketFile,"rb"); CHECK(f!=nullptr);
  char buffer[512]={0}; const size_t n=std::fread(buffer,1,sizeof(buffer)-1,f); std::fclose(f);
  PcStats s;
  CHECK(parsePcPacket(buffer,n,TOKEN,s)==PcPacketResult::Ok);  // Python hmac/hashlib == firmware SHA-256.
  CHECK(s.counter==1700000000123ULL && s.unixTime==1763600000UL && !std::strcmp(s.host,"DESKOS-TEST"));
  CHECK(s.cpuPermille==456 && s.ramPermille==623 && s.diskPermille==711 && s.upKBps==12 && s.batteryPercent==87);
}
void serviceChecks() {
  Fixture f; CHECK(f.monitor.hasToken());
  FakeNet net;
  CHECK(f.monitor.state(true)==PcLinkState::Waiting && f.monitor.state(false)==PcLinkState::NoNetwork);
  CHECK(f.send(sign(body(10)))==PcRx::Accepted && f.monitor.state(true)==PcLinkState::Connected);
  CHECK(f.send(sign(body(10)))==PcRx::Replay && f.send(sign(body(9)))==PcRx::Replay);
  CHECK(f.send(sign(body(11)),OUTSIDE)==PcRx::Source && f.send(sign(body(11)),LOCAL)==PcRx::Source);
  CHECK(f.send(sign(body(11)),0)==PcRx::Source);
  CHECK(f.send(sign(body(11)))==PcRx::Accepted);
  f.time.ok=true; f.time.now=1763600000UL+301;
  CHECK(f.send(sign(body(12)))==PcRx::Stale);
  f.time.now=1763600000UL+299; CHECK(f.send(sign(body(13)))==PcRx::Accepted);
  f.clock.time+=PcMonitorService::TIMEOUT_MS-1; CHECK(f.monitor.state(true)==PcLinkState::Connected);
  f.clock.time+=1; CHECK(f.monitor.state(true)==PcLinkState::Disconnected);  // Laptop off / agent stopped.
  CHECK(f.send(sign(body(14)))==PcRx::Accepted && f.monitor.state(true)==PcLinkState::Connected);
  f.clock.time+=1000;
  for (int i=0;i<25;++i) f.send("garbage");
  CHECK(f.monitor.lastReject()==PcRx::RateLimited);                            // Flood: cheap drop.
  f.clock.time+=1000; CHECK(f.send(sign(body(15)))==PcRx::Accepted);
  CHECK(f.monitor.accepted()==5 && f.monitor.rejected()>=30);
  // Regenerated token: old agents stop authenticating; record persisted first.
  const unsigned writes=f.store.writes;
  CHECK(f.monitor.regenerateToken() && f.store.writes==writes+1);
  CHECK(f.send(sign(body(16)))==PcRx::Auth && f.monitor.state(true)==PcLinkState::Waiting);
  // No token yet: nothing is accepted; zero RNG output is refused.
  TestClock c; FakeBlobStore s; FakeTime t; FakeRandom r; r.zeros=true; PcMonitorService m(s,c,t,r); m.begin();
  CHECK(!m.hasToken() && m.state(true)==PcLinkState::NoToken && !m.ensureToken());
  const std::string p=sign(body(1)); CHECK(m.receive(p.data(),p.size(),PEER,LOCAL,MASK)==PcRx::NoToken);
  r.zeros=false; CHECK(m.ensureToken() && m.hasToken());
  PcMonitorService again(s,c,t,r); again.begin(); CHECK(again.hasToken());     // Survives restart.
  char a[33],b[33]; m.tokenHex(a); again.tokenHex(b); CHECK(!std::strcmp(a,b));
  s.corrupt("deskos-pc","token",8); PcMonitorService broken(s,c,t,r); broken.begin(); CHECK(!broken.hasToken());
  (void)net;
}
struct Dummy : IApp {
  void onEnter() override { dirty_=true; }
  void onExit() override {}
  void update(uint32_t) override {}
  void render() override { dirty_=false; }
  void onTouch(const TouchEvent&) override {}
};
void screenStates() {
  Fixture f; FakeNet net; DisplayHAL display; PixelRenderer renderer(display); AppManager manager; Dummy dummy;
  PcMonitorScreen screen(renderer,manager,f.monitor,net,f.clock);
  for (uint8_t id=0;id<APP_COUNT;++id)
    manager.registerApp(static_cast<AppId>(id),id==static_cast<uint8_t>(AppId::PcMonitor)?static_cast<IApp&>(screen):static_cast<IApp&>(dummy));
  manager.open(AppId::PcMonitor); manager.update(0);
  auto& g=display.graphics();
  CHECK(screen.shownState()==PcLinkState::Waiting && g.outside==0);
  const unsigned long allocations=g_allocations;
  char packet[400];
  uint64_t n=100;
  for (int i=0;i<3000;++i) {
    f.clock.time+=20; manager.update(20);
    if (i<1000 && i%100==0) {
      std::snprintf(packet,sizeof(packet),"DPM1;n=%llu;t=1763600000;h=PC;cpu=%d;ram=950;ru=15000;rt=16000;dsk=999;up=-1;dn=-1;bat=-1;chg=0;",
                    static_cast<unsigned long long>(++n),i);
      uint8_t mac[32]; hmacSha256(TOKEN,16,packet,std::strlen(packet),mac);
      char full[400]; std::strcpy(full,packet); std::strcat(full,"mac=");
      static const char H[]="0123456789abcdef"; size_t len=std::strlen(full);
      for (int k=0;k<32;++k) { full[len++]=H[mac[k]>>4]; full[len++]=H[mac[k]&15]; }
      full[len]=0;
      f.monitor.receive(full,len,PEER,LOCAL,MASK);
    }
    if (i==1500) net.s=NetState::Connecting;
    if (i==2000) net.s=NetState::Connected;
  }
  CHECK(screen.shownState()==PcLinkState::Disconnected);  // Agent went silent: timeout shown.
  CHECK(g.outside==0 && g.clears==1 && g_allocations==allocations);
  const TouchEvent::Type seq[3]={TouchEvent::Type::Press,TouchEvent::Type::Release,TouchEvent::Type::Tap};
  for (auto t:seq) manager.onTouch({t,240,220,0,10});
  manager.update(0); CHECK(manager.activeId()==AppId::Home);
  manager.open(AppId::PcMonitor); manager.update(0);
  for (auto t:seq) manager.onTouch({t,80,220,0,10});
  manager.update(0); CHECK(manager.activeId()==AppId::Home);
}
}  // namespace
int main(int argc,char** argv) {
  if (argc>1) g_agentPacketFile=argv[1];
  struct Test { const char* name; void (*run)(); };
  const Test tests[]={
    {"PC1 SHA-256 FIPS vectors and HMAC RFC 4231 #1/#2/#6",shaVectors},
    {"PC2 DPM1 parse, tamper/wrong key, unknown/duplicate/range/size",packetParsing},
    {"PC3 cross-check: packet from the real Python agent verifies",crossCheckPythonAgent},
    {"PC4 replay, LAN source, clock skew, timeout, flood, token lifecycle",serviceChecks},
    {"PC5 dashboard states/timeout, bounds, HOME/BACK, no allocations",screenStates}
  };
  for (const auto& t:tests) { const int before=failures; t.run(); std::printf("%s: %s\n",failures==before?"PASS":"FAIL",t.name); }
  std::printf("RESULT: %u pc monitor tests, %d failures\n",static_cast<unsigned>(sizeof(tests)/sizeof(tests[0])),failures);
  return failures?1:0;
}
