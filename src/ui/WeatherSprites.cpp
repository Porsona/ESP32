#include "WeatherSprites.h"
#include <string.h>

namespace deskos {
namespace {
const char* const E="................";
const char* const SUN[2][16]={
  {".......g........","..g....g....g...","...g.......g....","......ggg.......",".....ggggg......",
   "....ggggggg.....","....ggggggg.....","gg..ggggggg..gg.","....ggggggg.....",".....ggggg......",
   "......ggg.......","...g.......g....","..g....g....g...",".......g........",E,E},
  {E,".......g........","...g...g...g....","......ggg.......",".....ggggg......",
   "....ggggggg.....","....ggggggg.....",".gg.ggggggg.gg..","....ggggggg.....",".....ggggg......",
   "......ggg.......","...g...g...g....",".......g........",E,E,E}
};
const char* const MOON[2][16]={
  {E,E,".......www......",".....www........","....www.....g...","....ww..........","...www..........",
   "...www..........","...www..........","....ww..........","....www.........",".....www........",
   ".......www......",E,E,E},
  {E,E,".......www......",".....www........","....www.........","....ww..........","...www..........",
   "...www.......g..","...www..........","....ww..........","....www.........",".....www....g...",
   ".......www......",E,E,E}
};
const char* const CLOUD[8]={
  "......++++......",".....+oooo+.....","..+++oooooo+....",".+ooooooooooo+..",
  "+oooooooooooooo+","+oooooooooooooo+",".+oooooooooooo+.","..++++++++++++.."
};
const char* const PARTLY_TOP[2][4]={
  {E,"..........ggg...",".........ggggg..","........ggggggg."},
  {"...........g....","..........ggg...",".........ggggg.g","........ggggggg."}
};
const char* const PARTLY_CLOUD_TOP[2]={"......++++gggg..",".....+oooo+ggg.."};
const char* const FOG[2][16]={
  {E,E,E,"..++++++++++....",E,"....++++++++++..",E,".++++++++++.....",E,"...++++++++++...",E,
   "..++++++++++....",E,E,E,E},
  {E,E,E,"...++++++++++...",E,"...++++++++++...",E,"..++++++++++....",E,"..++++++++++....",E,
   "...++++++++++...",E,E,E,E}
};
const char* const DROPS[2][3]={
  {"..*...*...*.....","....*...*...*...","..*...*...*....."},
  {"....*...*...*...","..*...*...*.....","....*...*...*..."}
};
const char* const FLAKES[2][3]={
  {"...w.....w......","......w.....w...","...w.....w......"},
  {E,"....w.....w.....",".......w.....w.."}
};
const char* const BOLT[6]={".......gg.......","......gg........",".....gggg.......",".......gg.......",
                           "......gg........","......g........."};
const char* const STORM_DROPS[2]={"..*.......*.....","....*.......*..."};

void fill(const char* rows[16]) { for (uint8_t i=0;i<16;++i) rows[i]=E; }
void cloudAt(const char* rows[16],uint8_t top) { for (uint8_t i=0;i<8;++i) rows[top+i]=CLOUD[i]; }
}  // namespace

WeatherIcon weatherIcon(WeatherKind kind,bool day,uint8_t frame,const char* rows[WEATHER_SPRITE_SIZE]) {
  const uint8_t f=frame&1;
  fill(rows);
  WeatherIcon icon{WeatherPalette::Cloud,false};
  switch (kind) {
    case WeatherKind::Clear:
      for (uint8_t i=0;i<16;++i) rows[i]=day?SUN[f][i]:MOON[f][i];
      icon.palette=WeatherPalette::Bright; break;
    case WeatherKind::PartlyCloudy:
      cloudAt(rows,4);
      for (uint8_t i=0;i<4;++i) rows[i]=PARTLY_TOP[f][i];
      rows[4]=PARTLY_CLOUD_TOP[0]; rows[5]=PARTLY_CLOUD_TOP[1];
      icon.palette=WeatherPalette::Bright; break;
    case WeatherKind::Fog:
      for (uint8_t i=0;i<16;++i) rows[i]=FOG[f][i];
      break;
    case WeatherKind::Drizzle:
    case WeatherKind::Rain:
      cloudAt(rows,2); rows[11]=DROPS[f][0]; rows[13]=DROPS[f][1]; rows[15]=DROPS[f][2];
      break;
    case WeatherKind::Snow:
      cloudAt(rows,2); rows[11]=FLAKES[f][0]; rows[13]=FLAKES[f][1]; rows[15]=FLAKES[f][2];
      break;
    case WeatherKind::Storm:
      cloudAt(rows,2);
      if (f==0) for (uint8_t i=0;i<6;++i) rows[10+i]=BOLT[i];
      else { rows[11]=STORM_DROPS[0]; rows[13]=STORM_DROPS[1]; }
      icon.palette=WeatherPalette::Storm; break;
    default:  // Cloudy / unknown: the cloud drifts one cell back and forth.
      cloudAt(rows,4); icon.drift=true; break;
  }
  return icon;
}

bool weatherSpritesWellFormed() {
  const char* rows[16];
  const WeatherKind kinds[9]={WeatherKind::Clear,WeatherKind::PartlyCloudy,WeatherKind::Cloudy,WeatherKind::Fog,
                              WeatherKind::Drizzle,WeatherKind::Rain,WeatherKind::Snow,WeatherKind::Storm,
                              WeatherKind::Unknown};
  for (WeatherKind k:kinds) for (uint8_t day=0;day<2;++day) for (uint8_t f=0;f<2;++f) {
    weatherIcon(k,day!=0,f,rows);
    for (const char* r:rows) if (!r || strlen(r)!=16) return false;
  }
  return true;
}
}  // namespace deskos
