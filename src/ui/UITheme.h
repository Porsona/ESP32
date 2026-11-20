#pragma once
#include <stdint.h>
#include "../core/AppTypes.h"

namespace deskos {
namespace theme {
constexpr uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b) {
  return static_cast<uint16_t>(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));
}
constexpr uint16_t Background=rgb565(0x17,0x13,0x29);
constexpr uint16_t Panel=rgb565(0x43,0x35,0x5D);
constexpr uint16_t Mint=rgb565(0x72,0xE6,0xBA);
constexpr uint16_t Gold=rgb565(0xF3,0xD3,0x7A);
constexpr uint16_t Rose=rgb565(0xF2,0x8F,0xAB);
constexpr uint16_t White=0xFFFF;
}
namespace layout {
constexpr Rect Tiles[6]={{8,40,96,72},{112,40,96,72},{216,40,96,72},
                         {8,120,96,72},{112,120,96,72},{216,120,96,72}};
constexpr Rect Back={0,200,156,40};
constexpr Rect Home={164,200,156,40};
static_assert(Tiles[5].x+Tiles[5].w<=320 && Tiles[5].y+Tiles[5].h<=200,"Home grid out of content bounds");
static_assert(Back.h>=40 && Home.h>=40,"Navigation hitboxes must be >=40px");
}
}  // namespace deskos
