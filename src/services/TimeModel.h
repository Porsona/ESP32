#pragma once
#include <stddef.h>
#include <stdint.h>

namespace deskos {
// Asia/Ho_Chi_Minh is UTC+7 all year (no DST). The ESP32 side also sets the
// POSIX TZ "<+07>-7" for libc; day/hour logic below is pure and host-tested.
constexpr int32_t LOCAL_UTC_OFFSET_S=7*3600;
constexpr char LOCAL_POSIX_TZ[]="<+07>-7";
constexpr uint32_t MIN_VALID_EPOCH=1735689600UL;  // 2025-01-01: anything earlier is "not synced".
inline bool plausibleEpoch(uint32_t epoch) { return epoch>=MIN_VALID_EPOCH; }
inline uint32_t localDayKey(uint32_t epoch) { return static_cast<uint32_t>((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)/86400ULL); }
inline uint8_t localHour(uint32_t epoch) { return static_cast<uint8_t>(((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)%86400ULL)/3600ULL); }
inline uint8_t localMinute(uint32_t epoch) { return static_cast<uint8_t>(((static_cast<uint64_t>(epoch)+LOCAL_UTC_OFFSET_S)%3600ULL)/60ULL); }

// --- Certificate validity window (UTC civil time as in mbedtls_x509_time) ---
// The Arduino-ESP32 2.0.11 mbedTLS build has MBEDTLS_HAVE_TIME_DATE off, so the
// handshake never checks notBefore/notAfter. HttpsFetchTask checks the leaf
// certificate itself with these pure helpers once NTP time is valid.
struct CivilTime { int year,mon,day,hour,min,sec; };
enum class CertDate : uint8_t { Valid, NotYetValid, Expired, BadDate };
inline bool civilToEpoch(const CivilTime& t,int64_t& out) {
  static const int DAYS[12]={31,29,31,30,31,30,31,31,30,31,30,31};
  if (t.year<1950 || t.year>9999 || t.mon<1 || t.mon>12 || t.day<1 || t.day>DAYS[t.mon-1] ||
      t.hour<0 || t.hour>23 || t.min<0 || t.min>59 || t.sec<0 || t.sec>60) return false;
  const bool leap=(t.year%4==0 && t.year%100!=0) || t.year%400==0;
  if (t.mon==2 && t.day==29 && !leap) return false;
  // Days from civil (proleptic Gregorian), H. Hinnant's algorithm.
  const int64_t y=t.year-(t.mon<=2?1:0);
  const int64_t era=(y>=0?y:y-399)/400;
  const int64_t yoe=y-era*400;
  const int64_t doy=(153*(t.mon+(t.mon>2?-3:9))+2)/5+t.day-1;
  const int64_t doe=yoe*365+yoe/4-yoe/100+doy;
  const int64_t days=era*146097+doe-719468;
  out=days*86400+t.hour*3600+t.min*60+(t.sec>59?59:t.sec);
  return true;
}
inline CertDate certDateStatus(const CivilTime& notBefore,const CivilTime& notAfter,uint32_t epoch) {
  int64_t from=0,to=0;
  if (!civilToEpoch(notBefore,from) || !civilToEpoch(notAfter,to) || from>to) return CertDate::BadDate;
  if (static_cast<int64_t>(epoch)<from) return CertDate::NotYetValid;
  if (static_cast<int64_t>(epoch)>to) return CertDate::Expired;
  return CertDate::Valid;
}
inline const char* certDateName(CertDate d) {
  return d==CertDate::Valid?"VALID":d==CertDate::NotYetValid?"NOT_YET_VALID":d==CertDate::Expired?"EXPIRED":"BAD_DATE";
}

class ITimeSource {
 public:
  virtual ~ITimeSource() = default;
  virtual bool valid() const = 0;      // Synced from NTP in this boot.
  virtual uint32_t epoch() const = 0;  // UTC seconds; meaningful only when valid().
};
}  // namespace deskos
