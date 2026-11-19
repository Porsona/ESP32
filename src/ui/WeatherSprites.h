#pragma once
#include <stdint.h>
#include "../services/WeatherModel.h"

namespace deskos {
constexpr uint8_t WEATHER_SPRITE_SIZE=16;
enum class WeatherPalette : uint8_t { Bright, Cloud, Storm };
// Two-frame 16x16 icon per weather kind (day/night for clear sky). `drift` asks
// the screen to animate by shifting the icon one cell instead of swapping art.
struct WeatherIcon { WeatherPalette palette; bool drift; };
WeatherIcon weatherIcon(WeatherKind kind,bool day,uint8_t frame,const char* rows[WEATHER_SPRITE_SIZE]);
bool weatherSpritesWellFormed();
}  // namespace deskos
