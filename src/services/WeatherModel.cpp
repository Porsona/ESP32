#include "WeatherModel.h"
#include <stdio.h>
#include <string.h>
#include "Crc32.h"

namespace deskos {
namespace {
constexpr uint32_t CACHE_MAGIC=0x57544831UL,LOCATION_MAGIC=0x574C4F31UL;  // "WTH1", "WLO1"

const char* findIn(const char* begin,const char* end,const char* needle) {
  const size_t n=strlen(needle);
  for (const char* p=begin;p+n<=end;++p) if (!memcmp(p,needle,n)) return p;
  return nullptr;
}
const char* skipSpace(const char* p,const char* end) {
  while (p<end && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n')) ++p;
  return p;
}
// JSON number -> value*10, rounded half away from zero. Accepts -?d+(.d+)?
bool parseDeci(const char* p,const char* end,int32_t& out) {
  bool negative=false;
  if (p<end && *p=='-') { negative=true; ++p; }
  if (p>=end || *p<'0' || *p>'9') return false;
  int64_t whole=0;
  while (p<end && *p>='0' && *p<='9') { whole=whole*10+(*p-'0'); if (whole>100000) return false; ++p; }
  int tenths=0,hundredths=0;
  if (p<end && *p=='.') {
    ++p;
    if (p>=end || *p<'0' || *p>'9') return false;
    tenths=*p++-'0';
    if (p<end && *p>='0' && *p<='9') hundredths=*p++-'0';
    while (p<end && *p>='0' && *p<='9') ++p;
  }
  if (p<end && (*p=='e' || *p=='E')) return false;  // Not used by Open-Meteo for these fields.
  int64_t value=whole*10+tenths+(hundredths>=5?1:0);
  out=static_cast<int32_t>(negative?-value:value);
  return true;
}
bool field(const char* begin,const char* end,const char* key,int32_t& out) {
  char quoted[40];
  snprintf(quoted,sizeof(quoted),"\"%s\"",key);
  const char* p=findIn(begin,end,quoted);
  if (!p) return false;
  p=skipSpace(p+strlen(quoted),end);
  if (p>=end || *p!=':') return false;
  return parseDeci(skipSpace(p+1,end),end,out);
}
bool labelChar(char c) { return (c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ' || c=='.' || c=='-' || c==','; }
}  // namespace

WeatherKind weatherKind(uint8_t c) {
  if (c==0 || c==1) return WeatherKind::Clear;
  if (c==2) return WeatherKind::PartlyCloudy;
  if (c==3) return WeatherKind::Cloudy;
  if (c==45 || c==48) return WeatherKind::Fog;
  if (c>=51 && c<=57) return WeatherKind::Drizzle;
  if ((c>=61 && c<=67) || (c>=80 && c<=82)) return WeatherKind::Rain;
  if ((c>=71 && c<=77) || c==85 || c==86) return WeatherKind::Snow;
  if (c==95 || c==96 || c==99) return WeatherKind::Storm;
  return WeatherKind::Unknown;
}
const char* weatherText(uint8_t c) {
  switch (c) {
    case 0: return "CLEAR SKY";
    case 1: return "MAINLY CLEAR";
    case 2: return "PARTLY CLOUDY";
    case 3: return "OVERCAST";
    case 45: case 48: return "FOG";
    case 51: case 53: case 55: return "DRIZZLE";
    case 56: case 57: return "ICY DRIZZLE";
    case 61: return "LIGHT RAIN";
    case 63: return "RAIN";
    case 65: return "HEAVY RAIN";
    case 66: case 67: return "FREEZING RAIN";
    case 71: return "LIGHT SNOW";
    case 73: return "SNOW";
    case 75: return "HEAVY SNOW";
    case 77: return "SNOW GRAINS";
    case 80: return "LIGHT SHOWERS";
    case 81: return "SHOWERS";
    case 82: return "HEAVY SHOWERS";
    case 85: case 86: return "SNOW SHOWERS";
    case 95: return "THUNDERSTORM";
    case 96: case 99: return "STORM + HAIL";
    default: return "UNKNOWN";
  }
}

bool parseOpenMeteo(const char* body,size_t length,WeatherReading& out) {
  if (!body || !length) return false;
  const char* end=body+length;
  const char* current=findIn(body,end,"\"current\"");  // Not "current_units".
  if (!current) return false;
  const char* p=skipSpace(current+9,end);
  if (p>=end || *p!=':') return false;
  p=skipSpace(p+1,end);
  if (p>=end || *p!='{') return false;
  const char* close=static_cast<const char*>(memchr(p,'}',end-p));
  if (!close) return false;  // Truncated body.
  int32_t temp=0,humidity=0,code=0,wind=0,isDay=0;
  if (!field(p,close,"temperature_2m",temp) || !field(p,close,"relative_humidity_2m",humidity) ||
      !field(p,close,"weather_code",code) || !field(p,close,"wind_speed_10m",wind) ||
      !field(p,close,"is_day",isDay)) return false;
  // Integers arrive as value*10 too: 85 -> 850.
  if (temp<-800 || temp>650 || humidity<0 || humidity>1000 || humidity%10 || code<0 || code>990 || code%10 ||
      wind<0 || wind>5000 || (isDay!=0 && isDay!=10)) return false;
  WeatherReading r;
  r.temperatureDeci=static_cast<int16_t>(temp); r.windDeci=static_cast<uint16_t>(wind);
  r.humidity=static_cast<uint8_t>(humidity/10); r.code=static_cast<uint8_t>(code/10);
  r.isDay=isDay?1:0; r.valid=1;
  out=r;
  return true;
}

WeatherLocation defaultLocation() {
  WeatherLocation l;
  memset(&l,0,sizeof(l));
  l.latE4=108231; l.lonE4=1066297;
  snprintf(l.label,sizeof(l.label),"%s","HO CHI MINH");
  return l;
}
bool validLocation(const WeatherLocation& l) {
  if (l.latE4<-900000 || l.latE4>900000 || l.lonE4<-1800000 || l.lonE4>1800000) return false;
  size_t n=0;
  while (n<sizeof(l.label) && l.label[n]) { if (!labelChar(l.label[n])) return false; ++n; }
  if (n==0 || n>=sizeof(l.label)) return false;
  for (size_t i=n;i<sizeof(l.label);++i) if (l.label[i]) return false;
  return true;
}
bool parseCoordinateE4(const char* text,int32_t limitE4,int32_t& out) {
  if (!text || !*text) return false;
  const char* p=text;
  bool negative=false;
  if (*p=='-' || *p=='+') negative=*p++=='-';
  int64_t whole=0; int digits=0;
  while (*p>='0' && *p<='9') { whole=whole*10+(*p++-'0'); if (++digits>3) return false; }
  if (!digits) return false;
  int64_t frac=0; int places=0;
  if (*p=='.') {
    ++p;
    while (*p>='0' && *p<='9') { if (places<4) { frac=frac*10+(*p-'0'); ++places; } ++p; }
  }
  if (*p) return false;
  while (places<4) { frac*=10; ++places; }
  const int64_t value=whole*10000+frac;
  if (value>limitE4) return false;
  out=static_cast<int32_t>(negative?-value:value);
  return true;
}
void formatCoordinate(int32_t e4,char* out,size_t size) {
  const uint32_t magnitude=static_cast<uint32_t>(e4<0?-static_cast<int64_t>(e4):e4);
  snprintf(out,size,"%s%lu.%04lu",e4<0?"-":"",static_cast<unsigned long>(magnitude/10000),
           static_cast<unsigned long>(magnitude%10000));
}
bool buildWeatherUrl(const WeatherLocation& l,char* out,size_t size) {
  if (!validLocation(l)) return false;
  char lat[16],lon[16];
  formatCoordinate(l.latE4,lat,sizeof(lat)); formatCoordinate(l.lonE4,lon,sizeof(lon));
  const int n=snprintf(out,size,
    "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
    "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
    "&wind_speed_unit=kmh&timezone=Asia%%2FHo_Chi_Minh",lat,lon);
  return n>0 && static_cast<size_t>(n)<size;
}

void sealWeatherCache(WeatherCacheRecord& r) { r.checksum=crc32Bytes(&r,offsetof(WeatherCacheRecord,checksum)); }
bool validWeatherCache(const WeatherCacheRecord& r) {
  const WeatherReading& w=r.reading;
  return r.magic==CACHE_MAGIC && r.version==WEATHER_SCHEMA_VERSION && r.bytes==sizeof(r) && w.valid==1 &&
         w.temperatureDeci>=-800 && w.temperatureDeci<=650 && w.humidity<=100 && w.code<=99 && w.isDay<=1 &&
         w.windDeci<=5000 && r.checksum==crc32Bytes(&r,offsetof(WeatherCacheRecord,checksum));
}
WeatherCacheRecord makeWeatherCache(const WeatherReading& reading,const WeatherLocation& l,uint32_t epoch) {
  WeatherCacheRecord r;
  memset(&r,0,sizeof(r));
  r.magic=CACHE_MAGIC; r.version=WEATHER_SCHEMA_VERSION; r.bytes=sizeof(r);
  r.reading=reading; r.latE4=l.latE4; r.lonE4=l.lonE4; r.fetchedEpoch=epoch;
  sealWeatherCache(r);
  return r;
}
void sealLocationRecord(WeatherLocationRecord& r) { r.checksum=crc32Bytes(&r,offsetof(WeatherLocationRecord,checksum)); }
bool validLocationRecord(const WeatherLocationRecord& r) {
  return r.magic==LOCATION_MAGIC && r.version==WEATHER_SCHEMA_VERSION && r.bytes==sizeof(r) &&
         validLocation(r.location) && r.checksum==crc32Bytes(&r,offsetof(WeatherLocationRecord,checksum));
}
WeatherLocationRecord makeLocationRecord(const WeatherLocation& l) {
  WeatherLocationRecord r;
  memset(&r,0,sizeof(r));
  r.magic=LOCATION_MAGIC; r.version=WEATHER_SCHEMA_VERSION; r.bytes=sizeof(r); r.location=l;
  sealLocationRecord(r);
  return r;
}
}  // namespace deskos
