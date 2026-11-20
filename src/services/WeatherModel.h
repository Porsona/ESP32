#pragma once
#include <stddef.h>
#include <stdint.h>
#include "BlobStore.h"

namespace deskos {
enum class WeatherKind : uint8_t { Clear, PartlyCloudy, Cloudy, Fog, Drizzle, Rain, Snow, Storm, Unknown };
WeatherKind weatherKind(uint8_t wmoCode);
const char* weatherText(uint8_t wmoCode);        // Short GLCD-friendly label.
inline bool wetWeather(WeatherKind k) { return k==WeatherKind::Drizzle || k==WeatherKind::Rain || k==WeatherKind::Storm; }

struct WeatherReading {
  int16_t temperatureDeci;  // 26.3 C -> 263
  uint16_t windDeci;        // km/h * 10
  uint8_t humidity,code,isDay,valid;
};
// Parses Open-Meteo "current" block. Tolerant of key order/whitespace; rejects
// missing fields and out-of-range values. No allocation.
bool parseOpenMeteo(const char* body,size_t length,WeatherReading& out);

struct WeatherLocation { int32_t latE4,lonE4; char label[16]; };
WeatherLocation defaultLocation();  // Ho Chi Minh City.
bool validLocation(const WeatherLocation& location);
bool parseCoordinateE4(const char* text,int32_t limitE4,int32_t& out);  // "10.8231" -> 108231
void formatCoordinate(int32_t e4,char* out,size_t size);
bool buildWeatherUrl(const WeatherLocation& location,char* out,size_t size);

constexpr uint16_t WEATHER_SCHEMA_VERSION=1;
struct WeatherCacheRecord {
  uint32_t magic;
  uint16_t version,bytes;
  WeatherReading reading;
  int32_t latE4,lonE4;
  uint32_t fetchedEpoch;    // 0 when the clock was not synced at fetch time.
  uint32_t checksum;
};
static_assert(sizeof(WeatherCacheRecord)==32,"Change weather schema version when the layout changes");
struct WeatherLocationRecord {
  uint32_t magic;
  uint16_t version,bytes;
  WeatherLocation location;
  uint32_t checksum;
};
static_assert(sizeof(WeatherLocationRecord)==36,"Change weather schema version when the layout changes");
void sealWeatherCache(WeatherCacheRecord& record);
bool validWeatherCache(const WeatherCacheRecord& record);
WeatherCacheRecord makeWeatherCache(const WeatherReading& reading,const WeatherLocation& location,uint32_t epoch);
void sealLocationRecord(WeatherLocationRecord& record);
bool validLocationRecord(const WeatherLocationRecord& record);
WeatherLocationRecord makeLocationRecord(const WeatherLocation& location);
}  // namespace deskos
