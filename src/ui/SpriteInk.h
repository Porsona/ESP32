#pragma once
#include <stdint.h>

namespace deskos {
// One sprite character and its RGB565 colour (multi-tone sprites beyond SpritePalette).
struct SpriteInk { char key; uint16_t color; };
}  // namespace deskos
